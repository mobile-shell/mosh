/*
    Mosh: the mobile shell

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

#include <cstdio>
#include <exception>
#include <string>

#include "src/crypto/crypto.h"
#include "src/protobufs/hostinput.pb.h"
#include "src/protobufs/userinput.pb.h"
#include "src/statesync/completeterminal.h"
#include "src/statesync/user.h"
#include "src/util/fatal_assert.h"

using Crypto::CryptoException;

static std::string user_resize_diff( const bool set_width, const int width, const bool set_height, const int height )
{
  ClientBuffers::UserMessage msg;
  ClientBuffers::Instruction* inst = msg.add_instruction();
  ClientBuffers::ResizeMessage* resize = inst->MutableExtension( ClientBuffers::resize );
  if ( set_width ) {
    resize->set_width( width );
  }
  if ( set_height ) {
    resize->set_height( height );
  }
  return msg.SerializeAsString();
}

static std::string host_resize_diff( const bool set_width, const int width, const bool set_height, const int height )
{
  HostBuffers::HostMessage msg;
  HostBuffers::Instruction* inst = msg.add_instruction();
  HostBuffers::ResizeMessage* resize = inst->MutableExtension( HostBuffers::resize );
  if ( set_width ) {
    resize->set_width( width );
  }
  if ( set_height ) {
    resize->set_height( height );
  }
  return msg.SerializeAsString();
}

static void expect_user_resize_rejected( const std::string& diff )
{
  bool got_exception = false;
  try {
    Network::UserStream user;
    user.apply_string( diff );
  } catch ( const CryptoException& e ) {
    got_exception = true;
    fatal_assert( !e.fatal );
  }
  fatal_assert( got_exception );
}

static void expect_host_resize_rejected( const std::string& diff )
{
  bool got_exception = false;
  try {
    Terminal::Complete terminal( 80, 24 );
    terminal.apply_string( diff );
  } catch ( const CryptoException& e ) {
    got_exception = true;
    fatal_assert( !e.fatal );
  }
  fatal_assert( got_exception );
}

static void test_user_resize_validation( void )
{
  Network::UserStream valid;
  valid.apply_string( user_resize_diff( true, 80, true, 24 ) );
  fatal_assert( valid.size() == 1 );

  Network::UserStream large_width;
  large_width.apply_string( user_resize_diff( true, 10000, true, 100 ) );
  fatal_assert( large_width.size() == 1 );

  Network::UserStream max_width;
  max_width.apply_string( user_resize_diff( true, 65535, true, 1 ) );
  fatal_assert( max_width.size() == 1 );

  Network::UserStream max_height;
  max_height.apply_string( user_resize_diff( true, 1, true, 65535 ) );
  fatal_assert( max_height.size() == 1 );

  Network::UserStream max_cells;
  max_cells.apply_string( user_resize_diff( true, 4096, true, 4096 ) );
  fatal_assert( max_cells.size() == 1 );

  expect_user_resize_rejected( user_resize_diff( false, 0, true, 24 ) );
  expect_user_resize_rejected( user_resize_diff( true, 80, false, 0 ) );
  expect_user_resize_rejected( user_resize_diff( true, 0, true, 24 ) );
  expect_user_resize_rejected( user_resize_diff( true, 80, true, 0 ) );
  expect_user_resize_rejected( user_resize_diff( true, 0, true, 0 ) );
  expect_user_resize_rejected( user_resize_diff( true, -1, true, 24 ) );
  expect_user_resize_rejected( user_resize_diff( true, 80, true, -1 ) );
  expect_user_resize_rejected( user_resize_diff( true, 65536, true, 1 ) );
  expect_user_resize_rejected( user_resize_diff( true, 1, true, 65536 ) );
  expect_user_resize_rejected( user_resize_diff( true, 4097, true, 4096 ) );
}

static void test_user_resize_serialization_validation( void )
{
  Network::UserStream valid;
  valid.push_back( Parser::Resize( 80, 24 ) );
  fatal_assert( !valid.diff_from( Network::UserStream() ).empty() );

  bool got_exception = false;
  Network::UserStream zero_width;
  zero_width.push_back( Parser::Resize( 0, 24 ) );
  try {
    zero_width.diff_from( Network::UserStream() );
  } catch ( const CryptoException& e ) {
    got_exception = true;
    fatal_assert( !e.fatal );
  }
  fatal_assert( got_exception );

  got_exception = false;
  try {
    Network::UserStream zero_size;
    zero_size.push_back( Parser::Resize( 0, 0 ) );
    zero_size.diff_from( Network::UserStream() );
  } catch ( const CryptoException& e ) {
    got_exception = true;
    fatal_assert( !e.fatal );
  }
  fatal_assert( got_exception );

  got_exception = false;
  try {
    Network::UserStream huge;
    huge.push_back( Parser::Resize( 4097, 4096 ) );
    huge.diff_from( Network::UserStream() );
  } catch ( const CryptoException& e ) {
    got_exception = true;
    fatal_assert( !e.fatal );
  }
  fatal_assert( got_exception );
}

static void test_host_resize_validation( void )
{
  Terminal::Complete valid( 80, 24 );
  valid.apply_string( host_resize_diff( true, 100, true, 30 ) );
  fatal_assert( valid.get_fb().ds.get_width() == 100 );
  fatal_assert( valid.get_fb().ds.get_height() == 30 );

  Terminal::Complete large_width( 80, 24 );
  large_width.apply_string( host_resize_diff( true, 10000, true, 100 ) );
  fatal_assert( large_width.get_fb().ds.get_width() == 10000 );
  fatal_assert( large_width.get_fb().ds.get_height() == 100 );

  Terminal::Complete max_width( 80, 24 );
  max_width.apply_string( host_resize_diff( true, 65535, true, 1 ) );
  fatal_assert( max_width.get_fb().ds.get_width() == 65535 );
  fatal_assert( max_width.get_fb().ds.get_height() == 1 );

  Terminal::Complete max_height( 80, 24 );
  max_height.apply_string( host_resize_diff( true, 1, true, 65535 ) );
  fatal_assert( max_height.get_fb().ds.get_width() == 1 );
  fatal_assert( max_height.get_fb().ds.get_height() == 65535 );

  Terminal::Complete max_cells( 80, 24 );
  max_cells.apply_string( host_resize_diff( true, 4096, true, 4096 ) );
  fatal_assert( max_cells.get_fb().ds.get_width() == 4096 );
  fatal_assert( max_cells.get_fb().ds.get_height() == 4096 );

  expect_host_resize_rejected( host_resize_diff( false, 0, true, 24 ) );
  expect_host_resize_rejected( host_resize_diff( true, 80, false, 0 ) );
  expect_host_resize_rejected( host_resize_diff( true, 0, true, 24 ) );
  expect_host_resize_rejected( host_resize_diff( true, 80, true, 0 ) );
  expect_host_resize_rejected( host_resize_diff( true, 0, true, 0 ) );
  expect_host_resize_rejected( host_resize_diff( true, -1, true, 24 ) );
  expect_host_resize_rejected( host_resize_diff( true, 80, true, -1 ) );
  expect_host_resize_rejected( host_resize_diff( true, 65536, true, 1 ) );
  expect_host_resize_rejected( host_resize_diff( true, 1, true, 65536 ) );
  expect_host_resize_rejected( host_resize_diff( true, 4097, true, 4096 ) );
}

int main( void )
{
  try {
    test_user_resize_validation();
    test_user_resize_serialization_validation();
    test_host_resize_validation();
  } catch ( const std::exception& e ) {
    fprintf( stderr, "resize FAILED: %s\n", e.what() );
    return 1;
  }

  printf( "resize PASSED\n" );
  return 0;
}
