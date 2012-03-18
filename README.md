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

Mosh does not support X forwarding or the non-interactive uses of SSH,
including port forwarding.

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

  Mosh is packaged for various operating systems.

  * [Debian][] unstable

        sudo apt-get install mosh

  * [Ubuntu][], through a PPA

        sudo add-apt-repository ppa:keithw/mosh
        sudo apt-get update
        sudo apt-get install mosh

  * [MacPorts][]

        sudo port install mosh

  * [Homebrew][]

        brew install mobile-shell

  [Debian]:   http://packages.debian.org/sid/mosh
  [Ubuntu]:   https://launchpad.net/~keithw/+archive/mosh
  [MacPorts]: https://trac.macports.org/browser/trunk/dports/net/mosh/Portfile
  [Homebrew]: http://mxcl.github.com/homebrew/

Building from source
--------------------

  On a Unix-like system you can build Mosh from source using the following
  commands:

    ./autogen.sh
    ./configure
    make
    make install   # as root

  `configure` accepts standard options, like `--prefix` to set the installation
  prefix.  Pass `--help` for a full listing.

  To build and use Mosh you will need

  * [GNU Autotools][]
  * the [Protocol Buffers][] library and compiler
  * [Boost][]
  * `ncurses`
  * `zlib`
  * the Perl module [IO::Pty][]

  including development packages where applicable.

  If `libutempter` is available, `mosh-server` will record sessions in the
  `utmp` file, which makes them visible to commands like `who`.

  The file `debian/control` contains a list of the relevant Debian packages.

  [GNU Autotools]:    http://www.gnu.org/software/autoconf/
  [Protocol Buffers]: http://code.google.com/p/protobuf/
  [Boost]:            http://www.boost.org/
  [IO::Pty]:          http://search.cpan.org/~toddr/IO-Tty/Pty.pm

Usage
-----

  The `mosh-client` binary must be installed on the user's machine, and
  the `mosh-server` binary on the remote host.

  The user runs:

    $ mosh [user@]host

  A command may also be specified, for example:

    $ mosh host -- screen -r

  If the `mosh-client` or `mosh-server` binaries are installed outside the
  user's PATH, `mosh` accepts the arguments `--client=PATH` and
  `--server=PATH` to select alternate locations. More options are
  documented in the mosh(1) manual page.

  Mosh supports 256-color mode as long as the user's own terminal
  does.  Generally this means the `TERM` environment variable must be
  set to `xterm-256color` or `screen-256color-bce` before running
  `mosh`.

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

  If the client changes IP addresses, the server will begin sending
  to the client on the new IP address within a few seconds.

  To function, Mosh requires UDP datagrams to be passed between client
  and server. By default, `mosh` uses a port number between 60000 and
  61000, but the user can select a particular port with the -p option.

Advice to distributors
----------------------

A note on compiler flags: Mosh is security-sensitive code. When making
automated builds for a binary package, we recommend passing the option
`--enable-compile-warnings=error` to ./configure. On GNU/Linux with
`g++` or `clang++`, the package should compile cleanly with
`-Werror`. Please report a bug if it doesn't.

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

More info
---------

  * Mosh Web site:

    <http://mosh.mit.edu>

  * `mosh-devel@mit.edu` mailing list:

    <http://mailman.mit.edu/mailman/listinfo/mosh-devel>

  * `mosh-users@mit.edu` mailing list:

    <http://mailman.mit.edu/mailman/listinfo/mosh-users>
