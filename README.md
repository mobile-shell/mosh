[![ci](https://github.com/mobile-shell/mosh/actions/workflows/ci.yml/badge.svg)](https://github.com/mobile-shell/mosh/actions/workflows/ci.yml)

Mosh: the mobile shell
======================

Mosh is a remote terminal application that supports intermittent
connectivity, allows roaming, and provides speculative local echo
and line editing of user keystrokes.

It aims to support the typical interactive uses of SSH, plus:

   * Mosh keeps the session alive if the client goes to sleep and
     wakes up later, or temporarily loses its Internet connection.

   * Mosh allows the client and server to "roam" and change IP
     addresses, while keeping the connection alive. Unlike SSH, Mosh
     can be used while switching between Wi-Fi networks or from Wi-Fi
     to cellular data to wired Ethernet.

   * The Mosh client runs a predictive model of the server's behavior
     in the background and tries to guess intelligently how each
     keystroke will affect the screen state. When it is confident in
     its predictions, it will show them to the user while waiting for
     confirmation from the server. Most typing and uses of the left-
     and right-arrow keys can be echoed immediately.

     As a result, Mosh is usable on high-latency links, e.g. on a
     cellular data connection or spotty Wi-Fi. In distinction from
     previous attempts at local echo modes in other protocols, Mosh
     works properly with full-screen applications such as emacs, vi,
     alpine, and irssi, and automatically recovers from occasional
     prediction errors within an RTT. On high-latency links, Mosh
     underlines its predictions while they are outstanding and removes
     the underline when they are confirmed by the server.

Mosh supports local TCP forwarding (`-L`) and local dynamic SOCKS5 CONNECT
forwarding (`-D`) over a Mosh-native UDP sidecar session. The `mosh-ssh`
entrypoint provides an OpenSSH-compatible subset for tools such as VS Code
Remote-SSH that need non-interactive command stdio plus local forwarding. Mosh
still does not support X forwarding, SSH agent forwarding, remote forwarding
(`-R`), or Unix socket forwarding.

Other features
--------------

   * Mosh adjusts its frame rate so as not to fill up network queues
     on slow links, so "Control-C" always works within an RTT to halt
     a runaway process.

   * Mosh warns the user when it has not heard from the server
     in a while.

   * Mosh supports lossy links that lose a significant fraction
     of their packets.

   * Mosh handles some Unicode edge cases better than SSH and existing
     terminal emulators by themselves, but requires a UTF-8
     environment to run.

   * Mosh leverages SSH to set up the connection and authenticate
     users. Mosh does not contain any privileged (root) code.

Getting Mosh
------------

  [The Mosh web site](https://mosh.org/#getting) has information about
  packages for many operating systems, as well as instructions for building
  from source.

  Note that `mosh-client` receives an AES session key as an environment
  variable.  If you are porting Mosh to a new operating system, please make
  sure that a running process's environment variables are not readable by other
  users.  We have confirmed that this is the case on GNU/Linux, OS X, and
  FreeBSD.

Usage
-----

  The `mosh-client` binary must exist on the user's machine, and the
  `mosh-server` binary on the remote host.

  The user runs:

    $ mosh [user@]host

  Local TCP forwarding can be requested with SSH-like options:

    $ mosh -L 127.0.0.1:8080:localhost:80 [user@]host
    $ mosh -D 127.0.0.1:1080 [user@]host

  `-L` and `-D` bind to `127.0.0.1` by default when no bind address is given.
  Dynamic forwarding implements SOCKS5 no-auth `CONNECT`; DNS names are sent to
  the server side for resolution.

  For VS Code Remote-SSH or other tools that expect an `ssh` binary, use
  `mosh-ssh`:

    $ mosh-ssh -T -D 127.0.0.1:1080 [user@]host sh

  In VS Code settings, point `remote.SSH.path` at the `mosh-ssh` binary and
  disable socket listen mode for this milestone:

    {
      "remote.SSH.path": "/path/to/mosh-ssh",
      "remote.SSH.enableDynamicForwarding": true,
      "remote.SSH.remoteServerListenOnSocket": false
    }

  If the `mosh-client` or `mosh-server` binaries live outside the user's
  `$PATH`, `mosh` accepts the arguments `--client=PATH` and `--server=PATH` to
  select alternate locations. More options are documented in the mosh(1) manual
  page.

  There are [more examples](https://mosh.org/#usage) and a
  [FAQ](https://mosh.org/#faq) on the Mosh web site.

How it works
------------

  The `mosh` program will SSH to `user@host` to establish the connection.
  SSH may prompt the user for a password or use public-key
  authentication to log in.

  From this point, `mosh` runs the `mosh-server` process (as the user)
  on the server machine. The server process listens on a high UDP port
  and sends its port number and an AES-128 secret key back to the
  client over SSH. The SSH connection is then shut down and the
  terminal session begins over UDP.

  When `-L` or `-D` forwarding is requested, `mosh-server` also starts a
  second encrypted UDP sidecar session and prints a `MOSH FORWARD` startup
  line. The forwarding sidecar carries independent reliable byte streams over
  Mosh UDP, so the forwarding connection roams with the Mosh client instead of
  depending on the bootstrap SSH connection.

  If the client changes IP addresses, the server will begin sending
  to the client on the new IP address within a few seconds.

  To function, Mosh requires UDP datagrams to be passed between client
  and server. By default, `mosh` uses a port number between 60000 and
  61000, but the user can select a particular port with the -p option.
  Please note that the -p option has no effect on the port used by SSH.

Advice to distributors
----------------------

A note on compiler flags: Mosh is security-sensitive code. When making
automated builds for a binary package, we recommend passing the option
`--enable-compile-warnings=error` to `./configure`. On GNU/Linux with
`g++` or `clang++`, the package should compile cleanly with
`-Werror`. Please report a bug if it doesn't.

Where available, Mosh builds with a variety of binary hardening flags
such as `-fstack-protector-all`, `-D_FORTIFY_SOURCE=2`, etc.  These
provide proactive security against the possibility of a memory
corruption bug in Mosh or one of the libraries it uses.  For a full
list of flags, search for `HARDEN` in `configure.ac`.  The `configure`
script detects which flags are supported by your compiler, and enables
them automatically.  To disable this detection, pass
`--disable-hardening` to `./configure`.  Please report a bug if you
have trouble with the default settings; we would like as many users as
possible to be running a configuration as secure as possible.

Mosh ships with a default optimization setting of `-O2`. Some
distributors have asked about changing this to `-Os` (which causes a
compiler to prefer space optimizations to time optimizations). We have
benchmarked with the included `src/examples/benchmark` program to test
this. The results are that `-O2` is 40% faster than `-Os` with g++ 4.6
on GNU/Linux, and 16% faster than `-Os` with clang++ 3.1 on Mac OS
X. In both cases, `-Os` did produce a smaller binary (by up to 40%,
saving almost 200 kilobytes on disk). While Mosh is not especially CPU
intensive and mostly sits idle when the user is not typing, we think
the results suggest that `-O2` (the default) is preferable.

Our Debian and Fedora packaging presents Mosh as a single package.
Mosh has a Perl dependency that is only required for client use.  For
some platforms, it may make sense to have separate mosh-server and
mosh-client packages to allow mosh-server usage without Perl.

Notes for developers
--------------------

To start contributing to Mosh, install the following dependencies:

Debian, Windows Subsystem for Linux:

```
$ sudo apt install -y build-essential protobuf-compiler \
    libprotobuf-dev pkg-config libutempter-dev zlib1g-dev libncurses5-dev \
    libssl-dev bash-completion tmux less
```

Fedora, RHEL:

```
$ sudo dnf group install development-tools
$ sudo dnf install automake protobuf-compiler protobuf-devel libutempter-devel \
    zlib-ng-compat-devel ncurses-devel openssl-devel bash-completion tmux less \
    perl-diagnostics
```

MacOS:

```
$ brew install protobuf automake
```

Once you have forked the repository, run the following to build and test Mosh:

```
$ ./autogen.sh
$ ./configure
$ make
$ make check
```

Mosh supports producing code coverage reports by tests, but this feature is
disabled by default. To enable it, make sure `lcov` is installed on your
system. Then, configure and run tests:

```
$ ./configure --enable-code-coverage
$ make check-code-coverage
```

This will run all tests and produce a coverage report in HTML form that can be
opened with your favorite browser. Ideally, newly added code should strive for
90% (or better) incremental test coverage.

More info
---------

  * Mosh Web site:

    <https://mosh.org>

  * `mosh-devel@mit.edu` mailing list:

    <https://mailman.mit.edu/mailman/listinfo/mosh-devel>

  * `mosh-users@mit.edu` mailing list:

    <https://mailman.mit.edu/mailman/listinfo/mosh-users>

  * `#mosh` channel on [Libera Chat](https://libera.chat/)

    https://web.libera.chat/#mosh
