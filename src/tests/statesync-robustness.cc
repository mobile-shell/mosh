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

/* Tests that a client applying host state from a malicious server does not
   abort: an unparseable diff is dropped (a caught CryptoException), host
   bytes that make the emulator want to reply (e.g. ESC[6n) are discarded,
   and a non-monotonic EchoAck is ignored -- none of which may crash the
   client. */

#include <cstdio>
#include <cstdlib>
#include <string>

#include "src/crypto/crypto.h"
#include "src/protobufs/hostinput.pb.h"
#include "src/statesync/completeterminal.h"

using namespace HostBuffers;

static int failures = 0;

static std::string make_hostbytes( const std::string& bytes )
{
  HostMessage m;
  m.add_instruction()->MutableExtension( hostbytes )->set_hoststring( bytes );
  return m.SerializeAsString();
}

static std::string make_echoack( uint64_t n )
{
  HostMessage m;
  m.add_instruction()->MutableExtension( echoack )->set_echo_ack_num( n );
  return m.SerializeAsString();
}

int main( void )
{
  /* 1. An unparseable diff must be dropped, not abort. */
  {
    Terminal::Complete c( 80, 24 );
    bool threw = false;
    try {
      c.apply_string( std::string( "\xff\xff\xff", 3 ) ); /* invalid protobuf wire */
    } catch ( const Crypto::CryptoException& ) {
      threw = true;
    }
    if ( !threw ) {
      fprintf( stderr, "FAIL: unparseable diff was not dropped\n" );
      failures++;
    }
  }

  /* 2. Host bytes that provoke a terminal reply (ESC[6n, a cursor position
        report) must be applied without aborting the client. */
  {
    Terminal::Complete c( 80, 24 );
    try {
      c.apply_string( make_hostbytes( "\x1b[6n" ) );
    } catch ( const Crypto::CryptoException& ) {
      fprintf( stderr, "FAIL: ESC[6n host bytes were rejected\n" );
      failures++;
    }
  }

  /* 3. A non-monotonic EchoAck must be ignored, not abort, and must not move
        echo_ack backwards. */
  {
    Terminal::Complete c( 80, 24 );
    c.apply_string( make_echoack( 5 ) );
    if ( c.get_echo_ack() != 5 ) {
      fprintf( stderr, "FAIL: EchoAck 5 not applied (got %llu)\n", (unsigned long long)c.get_echo_ack() );
      failures++;
    }
    c.apply_string( make_echoack( 3 ) ); /* non-monotonic */
    if ( c.get_echo_ack() != 5 ) {
      fprintf( stderr, "FAIL: non-monotonic EchoAck moved echo_ack to %llu\n", (unsigned long long)c.get_echo_ack() );
      failures++;
    }
  }

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
