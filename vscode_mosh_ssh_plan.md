# VS Code Remote-SSH over Mosh Forwarding

## Objective

Make VS Code Remote-SSH able to use a Mosh-backed transport without requiring
users to manually keep a separate `mosh -D` session open.

VS Code expects an OpenSSH-like executable. It launches that executable with a
shape similar to:

```sh
ssh -T -D 127.0.0.1:<port> [ssh options] user@host bash
```

The executable must provide:

- command stdin/stdout/stderr for the Remote-SSH bootstrap script;
- local dynamic forwarding for VS Code server connections;
- enough OpenSSH CLI compatibility to be configured as `remote.SSH.path`.

## User-Facing Shape

Add an executable named `mosh-ssh`.

Example VS Code setting:

```json
{
  "remote.SSH.path": "/opt/mosh/bin/mosh-ssh",
  "remote.SSH.enableDynamicForwarding": true,
  "remote.SSH.remoteServerListenOnSocket": false
}
```

`remote.SSH.remoteServerListenOnSocket` should be disabled for this milestone,
because this implementation carries TCP forwarding only. Unix-domain socket
forwarding remains a follow-up item.

## Bootstrap Flow

1. `mosh-ssh` parses the OpenSSH subset used by VS Code.
2. It runs ordinary `ssh` only long enough to start:

   ```sh
   mosh-server new --stdio-session --forward=streammux-v1 -- <command...>
   ```

3. `mosh-server` prints:

   ```text
   MOSH FORWARD <udp_port> <key> streammux-v1
   ```

4. The bootstrap SSH process exits.
5. `mosh-ssh` connects to the forwarding sidecar UDP port and opens:
   - one `STDIO` reliable stream for stdin/stdout;
   - one `STDERR` reliable stream for stderr;
   - any requested `-D` or `-L` local forwarding listeners.

## Protocol Usage

The existing forwarding sidecar protocol already has `OpenRequest.kind` values
for `STDIO` and `STDERR`. This milestone uses those values as follows:

- client-originated odd stream id for `STDIO`;
- client-originated odd stream id for `STDERR`;
- server pre-registers local endpoints for those incoming stream kinds;
- `OPEN_RESULT` confirms stream binding before data is delivered;
- `DATA`, `ACK`, `FIN`, and `RST` use the same reliable stream machinery as
  TCP forwarding.

No terminal state-sync session is created for `mosh-ssh`; only the forwarding
sidecar is used.

## OpenSSH CLI Subset

Implement the options needed by Remote-SSH and common SSH config flows:

- `-T`: accepted; no PTY is allocated.
- `-N`: no remote command; forwarding-only session.
- `-D [bind_address:]port`: local SOCKS5 CONNECT forwarding.
- `-L [bind_address:]port:host:hostport`: local direct TCP forwarding.
- `-p port`: bootstrap SSH port.
- `-l user`: bootstrap SSH user.
- `-F config`: pass through to bootstrap SSH.
- `-i identity_file`: pass through to bootstrap SSH.
- `-J jump`: pass through to bootstrap SSH.
- `-o key=value`: pass through common bootstrap SSH options.
- final `[user@]host [command...]`.

Remote forwarding (`-R`), X11 forwarding, SSH agent forwarding, ControlMaster,
and Unix-domain socket forwarding remain out of scope for this milestone.

## Server Stdio Mode

`mosh-server --stdio-session` must:

1. avoid `forkpty`;
2. start the requested command with ordinary pipes;
3. bridge child stdin/stdout to the `STDIO` stream;
4. bridge child stderr to the `STDERR` stream;
5. detach from the bootstrap SSH process after printing `MOSH FORWARD`;
6. keep the forwarding sidecar alive until the command and streams complete.

## Validation

Local validation should not require a real remote host:

1. Build and run the normal test suite:

   ```sh
   make -j"$(nproc)"
   make check
   ```

2. Use a local bootstrap mode for deterministic checks:

   ```sh
   ./src/frontend/mosh-ssh --local 127.0.0.1 sh -c 'printf stdout; printf stderr >&2'
   ```

3. Verify dynamic forwarding through the same `mosh-ssh` process:

   ```sh
   python3 -m http.server 18080 --bind 127.0.0.1
   ./src/frontend/mosh-ssh --local -N -D 127.0.0.1:18181 127.0.0.1
   curl --socks5-hostname 127.0.0.1:18181 http://127.0.0.1:18080/
   ```

4. Verify direct forwarding:

   ```sh
   ./src/frontend/mosh-ssh --local -N -L 127.0.0.1:18180:127.0.0.1:18080 127.0.0.1
   curl http://127.0.0.1:18180/
   ```

## Known Follow-Ups

- Preserve remote command exit status explicitly.
- Add Unix-domain socket forwarding for VS Code socket mode.
- Improve host resolution for SSH config aliases and ProxyJump-only hosts.
- Add remote forwarding (`-R`).
- Add automated integration tests around `mosh-ssh` stdio and forwarding.
