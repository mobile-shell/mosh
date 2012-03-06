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


Usage
-----

  The `mosh-client` binary must be installed on the user's machine, and
  the `mosh-server` binary on the remote host.

  The user runs:

    $ mosh [user@]host

  If the `mosh-client` or `mosh-server` binaries are installed outside the
  user's PATH, `mosh` accepts the arguments `--client=PATH` and
  `--server=PATH` to select alternate locations.


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

  To function, Mosh requires high-port UDP datagrams to be passed
  between client and server. Sites that have these ports firewalled
  would not be able to use Mosh.


More info
---------

  * Mosh Web site:

    <http://mosh.mit.edu>

  * `mosh-users@mit.edu` mailing list:

    <http://mailman.mit.edu/mailman/listinfo/mosh-users>
