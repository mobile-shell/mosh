/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

/* Integration regression test for the use-after-free in Transport::recv().

   We stand up a real server-side Transport (the victim) and, playing the role
   of a malicious client that holds the session key, hand-craft and encrypt
   Instructions and send them to the victim's UDP port.  First we build up the
   victim's received-state queue to {0, 1, 2} with a couple of well-formed
   states, then we send one Instruction with old_num = 1 and throwaway_num = 2:
   process_throwaway_until() frees the state-1 node that recv() still holds an
   iterator to, and the subsequent copy dereferences it.

   The fixed and unfixed code produce identical *output* here (both copy state
   1); only the memory safety differs, so this test is only meaningful under a
   memory checker.  It is built and run only with --enable-asan-tests, where
   the unfixed code reports a heap-use-after-free and the fixed code is clean.
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "src/crypto/crypto.h"
#include "src/network/network.h"
#include "src/network/networktransport-impl.h"
#include "src/network/transportfragment.h"
#include "src/protobufs/transportinstruction.pb.h"
#include "src/statesync/completeterminal.h"
#include "src/statesync/user.h"

/* Don't let the leak detector fail the test: the victim Transport and its
   sockets are intentionally not torn down.  A use-after-free is reported the
   moment it happens, independent of exit-time leak checking. */
extern "C" const char* __asan_default_options( void )
{
  return "detect_leaks=0";
}

using namespace Crypto;
using namespace Network;
using namespace TransportBuffers;

typedef Transport<Terminal::Complete, Network::UserStream> ServerTransport;

int main( void )
{
  /* Victim: a real server-side Transport bound to a loopback port. */
  Terminal::Complete local_terminal( 80, 24 );
  Network::UserStream remote_input;
  ServerTransport victim( local_terminal, remote_input, "127.0.0.1", NULL );

  const std::string key_str = victim.get_key();
  const int victim_port = atoi( victim.port().c_str() );

  /* Attacker socket + the shared key. */
  int attacker_fd = socket( AF_INET, SOCK_DGRAM, 0 );
  if ( attacker_fd < 0 ) {
    perror( "socket" );
    return EXIT_FAILURE;
  }
  struct sockaddr_in victim_addr;
  memset( &victim_addr, 0, sizeof( victim_addr ) );
  victim_addr.sin_family = AF_INET;
  victim_addr.sin_port = htons( victim_port );
  victim_addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );

  Session attacker_session( ( Base64Key( key_str ) ) );
  Fragmenter fragmenter;

  /* Encrypt one Instruction the way Connection::send() does and deliver it,
     then let the victim consume exactly that datagram. */
  bool send_ok = true;
  auto inject = [&]( uint64_t old_num, uint64_t new_num, uint64_t ack_num, uint64_t throwaway_num ) {
    Instruction inst;
    inst.set_protocol_version( MOSH_PROTOCOL_VERSION );
    inst.set_old_num( old_num );
    inst.set_new_num( new_num );
    inst.set_ack_num( ack_num );
    inst.set_throwaway_num( throwaway_num );
    /* empty diff: nothing to apply, so the copied state is used verbatim */

    std::vector<Fragment> frags = fragmenter.make_fragments( inst, 500 );
    for ( size_t i = 0; i < frags.size(); i++ ) {
      std::string payload = frags[i].tostring();
      Packet p( TO_SERVER, 1 /* timestamp */, uint16_t( -1 ) /* no reply */, payload );
      std::string datagram = attacker_session.encrypt( p.toMessage() );
      if ( sendto( attacker_fd,
                   datagram.data(),
                   datagram.size(),
                   0,
                   reinterpret_cast<struct sockaddr*>( &victim_addr ),
                   sizeof( victim_addr ) )
           != static_cast<ssize_t>( datagram.size() ) ) {
        perror( "sendto" );
        send_ok = false;
      }
    }

    /* deliver: recv() reads one datagram; retry until it arrives on loopback */
    for ( int tries = 0; tries < 2000; tries++ ) {
      try {
        victim.recv();
        return;
      } catch ( const NetworkException& ) {
        usleep( 1000 );
      }
    }
    fprintf( stderr, "FAIL: victim did not receive the injected datagram\n" );
    send_ok = false;
  };

  /* Build the received-state queue up to {0, 1, 2}. */
  inject( 0, 1, 0, 0 );
  inject( 1, 2, 0, 0 );

  if ( !send_ok ) {
    return EXIT_FAILURE;
  }
  if ( victim.get_remote_state_num() != 2 ) {
    fprintf( stderr,
             "FAIL: setup did not reach state 2 (got %llu)\n",
             (unsigned long long)victim.get_remote_state_num() );
    return EXIT_FAILURE;
  }

  /* The trigger: old_num = 1 with throwaway_num = 2 frees the reference node.
     Under AddressSanitizer the unfixed code aborts here with a
     heap-use-after-free; the fixed code completes cleanly. */
  inject( 1, 5, 0, 2 );

  if ( !send_ok ) {
    return EXIT_FAILURE;
  }

  printf( "no use-after-free detected\n" );
  return EXIT_SUCCESS;
}
