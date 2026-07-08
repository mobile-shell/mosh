# SSH Port Forwarding over Mosh

## Status

This is a technical design for adding SSH-style TCP port forwarding to this
repository. It is written as an implementation plan, not as a promise that the
upstream Mosh project would accept the same protocol changes.

The primary product target is making VS Code Remote-SSH able to use a
Mosh-backed transport when it needs a resilient connection to the remote
`vscode-server`.

## Problem Statement

Mosh currently uses SSH only as a bootstrap channel:

1. `scripts/mosh.pl` runs `ssh`.
2. The remote command starts `mosh-server`.
3. `mosh-server` prints `MOSH CONNECT <udp_port> <key>`.
4. The wrapper closes SSH and starts `mosh-client`.
5. All interactive terminal traffic moves to encrypted UDP.

This means OpenSSH port forwarding configured through `--ssh="ssh -L ..."` or
`--ssh="ssh -R ..."` disappears as soon as the bootstrap SSH process exits.
That is the core of GitHub issue mobile-shell/mosh#337.

Keeping the bootstrap SSH process alive is not a sufficient design. It would
fail the first time the client roams to a new address or the SSH TCP connection
breaks, which is exactly the network condition Mosh is meant to handle. Any
forwarding support worth adding must roam with the Mosh session.

## Evidence Reviewed

Relevant local Mosh code:

- `README.md` explicitly says Mosh does not support X forwarding or
  non-interactive SSH uses, including port forwarding.
- `scripts/mosh.pl` only uses SSH to start `mosh-server` and parse the startup
  line.
- `src/network/network.{h,cc}` provides the encrypted UDP datagram transport,
  client port hopping, server address reattachment, and MTU selection.
- `src/network/networktransport-impl.h` and
  `src/network/transportsender-impl.h` implement state synchronization, not a
  byte stream.
- `src/protobufs/transportinstruction.proto` carries one state-diff
  instruction, with `diff`, `ack_num`, `old_num`, `new_num`, and
  `throwaway_num`.
- `src/statesync/user.*` and `src/statesync/completeterminal.*` define the two
  synchronized objects: user input events and terminal framebuffer state.
- `src/frontend/stmclient.cc` and `src/frontend/mosh-server.cc` only select on
  UDP, stdin, and the PTY. There is no local listener, TCP dialer, or stream
  backpressure.

Relevant issue and prior art:

- mobile-shell/mosh#337: maintainers identify that true port forwarding needs
  roaming reliable byte streams, not a kept-alive SSH tunnel.
- mobile-shell/mosh#1111 and microsoft/vscode-remote-release#334: VS Code
  Remote-SSH needs SSH port forwarding to access its remote agent.
- mobile-shell/mosh#986: a prior "Multiple network stream support" PR tried to
  add a `MultiplexerStream` by aggregating state diffs. It was not wire
  compatible and was only intermediate work, not a complete forwarding
  implementation.
- `trzsz/tsshd`: a separate UDP SSH server inspired by Mosh. It supports SSH
  forwarding by exposing independent reliable streams over KCP or QUIC. Its
  forwarding design has three useful ideas:
  - each TCP forwarding connection maps to one independent reliable stream;
  - local forwarding sends a "dial" request to the server, then copies bytes
    between the stream and a `net.Conn`;
  - remote forwarding asks the server to listen, sends an accept id for each
    accepted connection, then the client opens a matching stream and claims it.

## Goals

1. Support Mosh-native roaming TCP forwarding.
2. Support at least the forwarding features VS Code Remote-SSH uses:
   - local dynamic SOCKS forwarding, equivalent to `ssh -D`;
   - local direct forwarding, equivalent to `ssh -L`;
   - non-interactive remote command stdio for `ssh -T ... <host> <command>`.
3. Preserve existing Mosh terminal behavior and local echo when forwarding is
   not requested.
4. Avoid putting arbitrary byte streams into `UserStream` or
   `Terminal::Complete` state diffs.
5. Bound memory and apply backpressure during disconnection, instead of
   buffering unbounded forwarded data.
6. Keep the implementation testable without a real Internet connection.

## Non-Goals for the First Implementation

The first implementation should not try to become a full OpenSSH replacement.
These can be follow-up items:

- X11 forwarding.
- SSH agent forwarding.
- UDP forwarding.
- Unix domain socket forwarding.
- SOCKS authentication.
- OpenSSH ControlMaster compatibility.
- Full parsing of every `ssh_config` option.
- Server roaming. Mosh currently supports client roaming; server address changes
  are outside this design.

## Design Summary

Add a new Mosh forwarding subsystem beside the existing terminal state
synchronization subsystem.

The key design choice is to use a reliable multiplexed byte-stream protocol over
Mosh's encrypted UDP datagram layer. Forwarding data must not be encoded as a
terminal state diff. It needs per-stream sequence numbers, ACKs, retransmission,
flow control, FIN/RST, and independent stream lifecycle.

For the first shippable version, run the forwarding subsystem as a sidecar UDP
session when forwarding is requested:

```
Existing interactive Mosh path:

  local terminal <-> UserStream/Complete <-> Network::Transport <-> UDP port A

Forwarding sidecar path:

  local TCP/SOCKS listeners <-> ForwardManager <-> Network::Connection <-> UDP port B
                                           |
                                           +-> reliable streams
```

This avoids destabilizing the existing state-synchronization protocol in normal
Mosh sessions. A future version can merge terminal state and forwarding into one
outer packet envelope on a single UDP port, but that is not required for a first
working VS Code path.

For VS Code Remote-SSH, add an SSH-compatible client entrypoint that speaks
enough OpenSSH command-line syntax to be configured as the VS Code SSH binary:

```
VS Code
  -> mosh-ssh -T -D <local_socks_port> [ssh options] host bash
  -> bootstrap through ordinary ssh
  -> remote mosh-server --stdio-session --forward
  -> close bootstrap ssh
  -> command stdio and -D traffic run over Mosh UDP reliable streams
```

## Why a Sidecar Forwarding Session First

There are two viable architecture shapes.

### Option A: One shared UDP port with a new packet envelope

This is the clean long-term architecture. The encrypted datagram payload would
become:

```
Envelope {
  version
  channel_type = state_sync | forward_control | forward_data
  payload
}
```

`Network::Transport` would become one channel inside a broader session manager,
and forwarding streams would be another channel.

Pros:

- One UDP port and one session key.
- Unified scheduling and congestion policy.
- Clean foundation for a future Mosh v2.

Cons:

- Requires refactoring `Network::Transport`, which currently owns its
  `Connection`.
- Requires a wire-protocol compatibility plan for every Mosh packet.
- Higher regression risk for the existing terminal path.

### Option B: Forwarding sidecar UDP session

When forwarding is requested, `mosh-server` creates a second encrypted UDP
`Network::Connection` dedicated to forwarding and prints a second startup line:

```
MOSH FORWARD <forward_port> <forward_key> streammux-v1
MOSH CONNECT <terminal_port> <terminal_key>
```

The existing terminal transport continues unchanged. The forwarding manager uses
the same lower-level Mosh UDP encryption, timestamping, MTU, and roaming
behavior, but it owns its own reliable-stream protocol.

Pros:

- Minimal risk to existing Mosh sessions.
- Forwarding can be developed and tested independently.
- Normal sessions remain wire-compatible.
- VS Code's non-interactive mode can use only the forwarding session.

Cons:

- Interactive sessions with forwarding need one extra UDP port.
- Scheduling between terminal and forwarding traffic happens at the OS/network
  level, not inside one packet scheduler.
- A later single-port merge would still be useful.

Recommendation: implement Option B first. Keep Option A as the cleanup path once
the forwarding semantics are proven.

## Protocol Overview

The forwarding sidecar protocol runs over `Network::Connection`, so each packet
is already encrypted and authenticated with the Mosh session key. The new layer
adds reliability and multiplexing.

Add a new protobuf file, for example `src/protobufs/forward.proto`:

```proto
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package ForwardBuffers;

message ForwardPacket {
  optional uint32 version = 1;
  optional uint64 packet_id = 2;
  repeated ForwardFrame frame = 3;
}

message ForwardFrame {
  enum Type {
    HELLO = 0;
    HELLO_OK = 1;
    OPEN = 2;
    OPEN_RESULT = 3;
    DATA = 4;
    ACK = 5;
    WINDOW_UPDATE = 6;
    FIN = 7;
    RST = 8;
    LISTEN = 9;
    LISTEN_RESULT = 10;
    PING = 11;
    PONG = 12;
  }

  optional Type type = 1;
  optional uint64 stream_id = 2;
  optional uint64 seq = 3;
  optional uint64 ack = 4;
  optional uint32 recv_window = 5;
  optional bytes data = 6;
  optional OpenRequest open = 7;
  optional ListenRequest listen = 8;
  optional uint32 error_code = 9;
  optional string error_message = 10;
}

message OpenRequest {
  enum Kind {
    DIRECT_TCP = 1;
    REMOTE_ACCEPTED_TCP = 2;
    STDIO = 3;
    STDERR = 4;
  }

  optional Kind kind = 1;
  optional string host = 2;
  optional uint32 port = 3;
  optional string origin_host = 4;
  optional uint32 origin_port = 5;
}

message ListenRequest {
  enum Kind {
    REMOTE_TCP = 1;
  }

  optional Kind kind = 1;
  optional uint64 listener_id = 2;
  optional string bind_host = 3;
  optional uint32 bind_port = 4;
  optional string connect_host = 5;
  optional uint32 connect_port = 6;
}
```

Implementation can tune field names, but the protocol must keep these concepts:

- `stream_id`: unique per reliable byte stream. Client-originated streams use
  odd ids, server-originated streams use even ids.
- `seq`: byte offset for `DATA` and `FIN`.
- `ack`: cumulative byte offset received contiguously.
- `recv_window`: remaining bytes the peer may send.
- `OPEN_RESULT`: success or failure for a stream open.
- `RST`: immediate teardown and error propagation.
- `LISTEN`: remote listener setup for `-R`.

### Reliability

Each stream maintains:

- send next sequence;
- oldest unacknowledged sequence;
- retransmission queue of sent segments;
- receive next contiguous sequence;
- out-of-order receive map;
- peer receive window;
- local receive window;
- state: opening, open, local-fin, remote-fin, closing, closed, reset.

DATA handling:

1. Split writes into segments no larger than the forwarding MTU after protobuf
   overhead.
2. Store each sent segment until cumulatively acknowledged.
3. Retransmit unacknowledged segments on RTO.
4. Accept duplicate DATA and ACK it without delivering twice.
5. Buffer out-of-order DATA only up to the per-stream receive cap.
6. Deliver bytes to the local TCP socket only in order.

RTO should reuse `Network::Connection` RTT estimates where possible. Start with
the existing Mosh range of 50 ms to 1000 ms, then back off per stream under
loss. The implementation can begin with cumulative ACK only. SACK can be added
later if high-loss throughput is poor.

### Flow Control and Buffer Limits

Forwarding is dangerous without memory bounds. A paused VS Code file transfer
or a disconnected client can produce much more data than terminal typing.

Use all of these limits:

- per-stream send buffer cap, default 4 MiB;
- per-stream receive buffer cap, default 4 MiB;
- global forwarding buffer cap, default 64 MiB;
- maximum open streams, default 256;
- maximum queued remote accepts, default 64 per listener;
- connection idle timeout for streams with no local peer, default 30 seconds.

When a cap is reached:

- stop reading from the local TCP socket if backpressure can solve it;
- if the local peer keeps writing after the stream is blocked, let the local TCP
  window apply pressure;
- if memory is still exceeded, send `RST` for the lowest-priority stream.

Terminal traffic must stay responsive. In interactive Mosh sessions, the
forwarding manager should have a lower scheduling priority than terminal state
updates.

### Packet Scheduling

`ForwardManager::tick()` should build packets in priority order:

1. control frames: HELLO, OPEN_RESULT, RST, FIN;
2. ACK and WINDOW_UPDATE;
3. retransmissions whose RTO expired;
4. new DATA, round-robin across streams.

Limit each UDP datagram to the MTU reported by `Network::Connection::get_MTU()`.
Unlike `TransportFragment`, forwarding DATA should not require reassembly of a
single giant protobuf. Segment the stream data before encoding frames.

### Roaming

Client roaming falls out of `Network::Connection`:

- the client keeps sending authenticated datagrams from the new address;
- the server accepts the latest valid client source address;
- forwarding stream ids, sequence numbers, and retransmission queues survive;
- no SSH reauthentication is needed after bootstrap.

The forwarding manager should send PING frames while any listener or stream is
active, even if no DATA is pending. This gives the server enough packets to
reattach after a roam.

## Forwarding Semantics

### Local Forwarding, `-L`

For:

```
mosh -L [bind_address:]local_port:target_host:target_port user@host
```

client behavior:

1. Bind a local TCP listener. Default bind address is loopback only.
2. On accept, allocate a client-originated stream id.
3. Send `OPEN { kind=DIRECT_TCP, host=target_host, port=target_port }`.
4. Wait for `OPEN_RESULT`.
5. If successful, copy local TCP bytes into stream DATA and stream DATA into the
   local TCP socket.

server behavior:

1. On `OPEN DIRECT_TCP`, resolve and dial `target_host:target_port` from the
   server side.
2. Return `OPEN_RESULT`.
3. Copy stream bytes to the TCP socket and TCP socket bytes to stream DATA.

This is the direct equivalent of OpenSSH `direct-tcpip`.

### Dynamic Forwarding, `-D`

For:

```
mosh-ssh -D [bind_address:]local_port user@host
```

client behavior:

1. Bind a local TCP listener. Default bind address is loopback only.
2. For each accepted connection, parse SOCKS locally.
3. Support SOCKS5 no-auth `CONNECT` for IPv4, IPv6, and domain-name targets.
4. For each SOCKS CONNECT target, open a `DIRECT_TCP` stream to the server.
5. Send the SOCKS success response only after `OPEN_RESULT` succeeds.

Unsupported SOCKS commands should fail cleanly:

- `BIND`: not supported.
- `UDP ASSOCIATE`: not supported in the first implementation.
- authenticated SOCKS methods: not supported.

Remote DNS is important. If the SOCKS target is a domain name, send the domain
name to the server and let the server resolve it. That matches the behavior VS
Code expects when it asks a local dynamic forwarding port to reach a remote
localhost service.

### Remote Forwarding, `-R`

Remote forwarding is not required for the first VS Code path, but the protocol
should support it.

For:

```
mosh -R [bind_address:]remote_port:target_host:target_port user@host
```

startup behavior:

1. Client sends `LISTEN { kind=REMOTE_TCP, bind_host, bind_port, connect_host,
   connect_port }`.
2. Server binds the remote listener.
3. Server returns `LISTEN_RESULT`.
4. If `ExitOnForwardFailure=yes` was requested, startup fails if any listener
   fails.

on remote accept:

1. Server accepts a TCP connection.
2. Server allocates a server-originated stream id.
3. Server sends `OPEN { kind=REMOTE_ACCEPTED_TCP, origin_host, origin_port,
   host=connect_host, port=connect_port }`.
4. Client dials `connect_host:connect_port` locally.
5. Client sends `OPEN_RESULT`.
6. Bytes are copied both ways.

This differs from `tsshd`'s accept-id pattern because this protocol can let
either side originate a stream directly. The accept-id pattern is still useful
if the implementation later separates "control stream" creation from "data
stream" creation.

## VS Code Remote-SSH Mode

VS Code cannot use the existing `mosh` wrapper as a drop-in SSH binary. The
current CLI is terminal-oriented and has conflicting option semantics such as
`-p` meaning Mosh UDP port, while OpenSSH uses `-p` for SSH port.

Add a new executable or installed wrapper named `mosh-ssh`.

`mosh-ssh` should implement the OpenSSH subset needed by Remote-SSH:

- `-T`: no remote PTY.
- `-D [bind_address:]port`: local dynamic forwarding.
- `-L [bind_address:]port:host:hostport`: local forwarding.
- `-R [bind_address:]port:host:hostport`: remote forwarding, phase 2.
- `-N`: no remote command, forwarding only.
- `-p port`: SSH bootstrap port.
- `-l user`: SSH bootstrap user.
- `-F config`: pass to bootstrap ssh.
- `-i identity_file`: pass to bootstrap ssh.
- `-J jump`: pass to bootstrap ssh initially; true UDP forwarding through
  ProxyJump can be designed later.
- `-o key=value`: pass through common bootstrap options and locally interpret
  `ExitOnForwardFailure`.
- final `[user@]host [command...]`.

The bootstrap still uses ordinary SSH for authentication and to start
`mosh-server`. After that, the SSH connection exits and all command stdio and
forwarding streams run over Mosh UDP.

### Remote Command Execution

VS Code usually starts Remote-SSH with no TTY and runs a shell/bootstrap
command. A PTY can corrupt machine-readable bootstrap output, so `mosh-server`
needs a non-PTY session mode:

```
mosh-server new --stdio-session --forward ...
```

In stdio mode:

1. Do not call `forkpty`.
2. Use pipes for child stdin, stdout, and stderr.
3. Map stdin/stdout to a reliable stream with `OpenRequest.kind=STDIO`.
4. Map stderr to a separate reliable stream with `OpenRequest.kind=STDERR`, or
   merge it only behind an explicit compatibility flag.
5. Preserve exit status and signal reporting so `mosh-ssh` exits like `ssh`.

The local `mosh-ssh` process connects its own stdin/stdout/stderr to these
streams. This gives VS Code the scriptable SSH-like process it expects.

### Expected VS Code Flow

The intended successful flow is:

```
VS Code launches:
  mosh-ssh -T -D 127.0.0.1:<port> remote-host bash

mosh-ssh:
  1. starts bootstrap ssh to remote-host;
  2. starts mosh-server --stdio-session --forward on the remote;
  3. parses MOSH FORWARD;
  4. starts the local SOCKS listener;
  5. proxies stdio over reliable streams;
  6. serves SOCKS CONNECT requests by opening DIRECT_TCP streams.

Remote VS Code server:
  listens on localhost:<random_port> or a socket.

VS Code client:
  connects through local SOCKS port to remote localhost:<random_port>.
```

If VS Code is configured to use "remote server listen on socket", Unix socket
forwarding will be needed. That can be a phase 2 feature after TCP mode works.

## Code Changes

### New Directories and Files

Proposed new C++ files:

- `src/forward/forwardmanager.h`
- `src/forward/forwardmanager.cc`
- `src/forward/reliablestream.h`
- `src/forward/reliablestream.cc`
- `src/forward/tcpforward.h`
- `src/forward/tcpforward.cc`
- `src/forward/socks.h`
- `src/forward/socks.cc`
- `src/protobufs/forward.proto`

Optional new frontend:

- `src/frontend/mosh-ssh.cc`

Build updates:

- `src/Makefile.am`
- `src/protobufs/Makefile.am`
- `src/frontend/Makefile.am`
- new `src/forward/Makefile.am`

### Network Layer Refactor

`Network::Connection` is already close to what the forwarding manager needs:

- encrypted authenticated datagrams;
- server-side client address reattachment;
- client-side port hopping;
- `fds()`;
- MTU reporting;
- RTT estimate.

Small changes needed:

1. Make sure `get_MTU()` is public and usable by forwarding code.
2. Add a non-fatal send result API, for example:

   ```c++
   bool send_datagram(const std::string& payload);
   ```

   The current `send()` records `send_error` but returns `void`.
3. Keep the existing `send()` and `recv()` behavior for `Network::Transport`.
4. Do not require `Network::Transport` changes for normal sessions.

### Event Loop Changes

Forwarding requires nonblocking TCP sockets and write readiness. The current
`Select` wrapper only tracks read fds.

Extend `src/util/select.h` to support:

- read fds;
- write fds;
- ready-read query;
- ready-write query.

The forwarding manager should expose:

```c++
std::vector<int> read_fds() const;
std::vector<int> write_fds() const;
int wait_time() const;
void handle_readable(int fd);
void handle_writable(int fd);
void tick();
```

Interactive client loop integration:

- `STMClient::main()` adds forwarding fds if forwarding was configured.
- Terminal stdin and UDP handling stay higher priority.
- `network->tick()` and `forward_manager.tick()` both run each loop.

Server loop integration:

- `serve()` adds forwarding fds beside the UDP fd and PTY fd.
- If forwarding remains active after the PTY exits, the default behavior should
  still shut down with the Mosh session unless `-N` or stdio mode requested a
  forward-only session.

### Bootstrap Changes

Existing `MOSH CONNECT` parsing should stay compatible for normal sessions.

When forwarding is requested:

1. Client wrapper passes a new flag to `mosh-server`, for example:

   ```
   mosh-server new --forward=streammux-v1 ...
   ```

2. Server starts the forwarding sidecar connection before detaching.
3. Server prints:

   ```
   MOSH FORWARD <port> <key> streammux-v1
   MOSH CONNECT <port> <key>
   ```

   `MOSH CONNECT` should remain exact for existing parser expectations.
4. New clients parse `MOSH FORWARD`; old clients will only see this line if they
   explicitly requested a new server flag, so backward compatibility is not
   affected.
5. If forwarding was requested but no `MOSH FORWARD` line appears, fail early
   with a clear message.

For `mosh-ssh`, the process can skip the terminal `MOSH CONNECT` line entirely
in a future optimization, but using both lines initially keeps startup
consistent.

## Security Model

Authentication remains SSH bootstrap plus a Mosh session key. The forwarding
sidecar has its own random key printed over the authenticated SSH channel.

Default binding rules:

- `-L` and `-D` bind to loopback unless the user explicitly supplies another
  bind address.
- `-R` binds to loopback on the server unless explicitly configured otherwise.
- Gateway-style remote binds should require an explicit option.

Server-side dialing:

- `DIRECT_TCP` dials as the logged-in user on the remote host.
- This is comparable to what the user could do from the shell with `nc` or
  `curl`.
- The first implementation does not need to enforce `sshd_config`
  `AllowTcpForwarding`, because `mosh-server` is not `sshd` and may not have
  permission to read the effective matched sshd config. A later hardening pass
  can add optional config checks, following the `tsshd` model.

Local SOCKS:

- no authentication;
- loopback bind by default;
- remote DNS for domain names;
- fail unsupported commands without leaking partial connections.

Resource limits:

- enforce stream and global memory caps;
- enforce max streams and max listeners;
- close unclaimed remote accepts;
- log repeated failures at a rate-limited level.

Replay and injection:

- `Network::Connection` already rejects wrong packet direction and uses
  authenticated encryption.
- The forwarding protocol must also reject old control frames by stream state
  and sequence numbers.

## Failure Behavior

### Temporary Network Loss

Local TCP reads pause when stream send windows are full. Existing local TCP
clients, including VS Code, will block naturally rather than causing Mosh to
buffer without bound.

When the Mosh UDP path resumes, retransmission continues from the oldest
unacknowledged byte.

### Long Disconnection

If buffers remain full past a configurable timeout, reset lower-priority streams
first. Do not kill the terminal session just because a forwarded stream is
overloaded.

For `mosh-ssh` stdio mode, if the primary stdio stream resets, exit the whole
process because VS Code expects the SSH command to be the session owner.

### Remote Dial Failure

Map common errors to OpenSSH-like failure behavior:

- connection refused: SOCKS reply `connection refused`, stream `OPEN_RESULT`
  failure;
- DNS failure: SOCKS reply `host unreachable`;
- timeout: SOCKS reply `TTL expired` or generic failure;
- permission denied: generic failure.

### Listener Failure

For local `-L` or `-D`, fail before bootstrap if the local listener cannot bind.

For remote `-R`, send `LISTEN` during startup and wait for `LISTEN_RESULT`.
Honor `ExitOnForwardFailure=yes` when supplied through `-o`.

## Implementation Phases

### Phase 0: Protocol and Harness

- Add `forward.proto`.
- Add a fake datagram endpoint for deterministic tests.
- Implement stream state machine with packet loss, reordering, duplication, and
  retransmission tests.
- No TCP sockets yet.

### Phase 1: Local TCP and Dynamic Forwarding

- Implement client-side `-L`.
- Implement client-side SOCKS5 `-D`.
- Implement server-side direct TCP dial.
- Integrate with `mosh --local` for tests.
- Add bootstrap `MOSH FORWARD` line and parser.
- Add loopback bind defaults and buffer limits.

This phase gives a manually usable roaming tunnel and covers the core byte
stream protocol.

### Phase 2: `mosh-ssh` for VS Code

- Add `mosh-ssh` OpenSSH-subset CLI parser.
- Add `mosh-server --stdio-session`.
- Add reliable stdio and stderr streams.
- Support `ssh -T -D ... host command` shape.
- Run VS Code Remote-SSH smoke tests against TCP-mode `vscode-server`.

This phase is the first VS Code-ready milestone.

### Phase 3: Remote Forwarding

- Add `-R` listener requests.
- Add server-originated streams.
- Add `ExitOnForwardFailure`.
- Add accept timeout and max pending accept limits.

### Phase 4: Hardening and Compatibility

- Add optional `sshd_config` style checks if desired.
- Add Unix domain socket forwarding for VS Code socket mode.
- Add SACK if throughput under loss is poor.
- Consider merging sidecar and terminal traffic into a single UDP port.

## Test Plan

Unit tests:

- stream open success and failure;
- in-order DATA delivery;
- duplicate DATA;
- out-of-order DATA;
- cumulative ACK;
- retransmission after packet loss;
- FIN half-close both directions;
- RST cleanup;
- receive window exhaustion;
- global memory cap enforcement;
- SOCKS5 parser for IPv4, IPv6, and domain-name CONNECT.

Integration tests:

- local echo server through `-L`;
- HTTP request through `-D` SOCKS;
- remote dial failure propagates to SOCKS;
- simulated packet loss and reordering with a fake datagram endpoint;
- client port hop while a forwarded TCP stream remains open;
- remote listener `-R` accept path after phase 3.

End-to-end tests:

- `mosh --local -L ...` against a local TCP echo service;
- `mosh-ssh -T -D ... localhost bash -lc '...'` and then connect through the
  SOCKS listener;
- VS Code Remote-SSH smoke:
  - configure `remote.SSH.path` to `mosh-ssh`;
  - connect to a test Linux host;
  - verify the server install script completes;
  - open a workspace;
  - run a terminal command;
  - use VS Code port forwarding for a remote HTTP server.

Regression tests:

- normal `mosh localhost` still uses the old protocol path;
- old-style startup without forwarding does not emit `MOSH FORWARD`;
- `make check` existing terminal/state tests still pass.

## Open Questions

1. Should the first user-facing command be named `mosh-ssh`, `mosh-proxy`, or a
   `mosh --ssh-compatible` mode? A separate `mosh-ssh` binary avoids conflicts
   with existing `mosh -p` semantics.
2. Should interactive `mosh -L/-D` be supported in the first release, or should
   all forwarding initially go through `mosh-ssh`? Supporting both shares most
   code but increases CLI work.
3. Should the forwarding sidecar reuse the same Mosh UDP port in phase 1? This
   is cleaner operationally but requires larger refactoring. The recommended
   first cut uses a second UDP port.
4. How much OpenSSH config parsing is necessary for VS Code in practice? The
   minimal parser can pass most options to bootstrap `ssh`, but it must locally
   understand the forwarding options.
5. Should stdio mode keep stderr separate from stdout? For VS Code, separate
   streams are safer because bootstrap output can be machine parsed.

## Recommended First Milestone

Build Phase 0 and Phase 1 first:

- forwarding sidecar UDP session;
- reliable stream manager;
- `-L` direct TCP;
- `-D` SOCKS5;
- local and server integration tests.

Then build `mosh-ssh` and stdio mode as Phase 2. That is the point where VS Code
Remote-SSH can be validated end to end.

This sequencing avoids the trap identified in issue #337: it does not keep SSH
alive as a fragile tunnel. It gives forwarding the same client-roaming property
as Mosh itself, while keeping normal terminal sessions on the existing stable
path until the new stream protocol is proven.
