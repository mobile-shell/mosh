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

#include <assert.h>

#include "byteorder.h"
#include "transportfragment.h"
#include "transportinstruction.pb.h"
#include "compressor.h"
#include "fatal_assert.h"

using namespace Network;
using namespace TransportBuffers;

static string network_order_string( uint16_t host_order )
{
  uint16_t net_int = htobe16( host_order );
  return string( (char *)&net_int, sizeof( net_int ) );
}

static string network_order_string( uint64_t host_order )
{
  uint64_t net_int = htobe64( host_order );
  return string( (char *)&net_int, sizeof( net_int ) );
}

string Fragment::tostring( void )
{
  assert( initialized );

  string ret;
  
  ret += network_order_string( id );

  fatal_assert( !( fragment_num & 0x8000 ) ); /* effective limit on size of a terminal screen change or buffered user input */
  uint16_t combined_fragment_num = ( final << 15 ) | fragment_num;
  ret += network_order_string( combined_fragment_num );

  assert( ret.size() == frag_header_len );

  ret += contents;

  return ret;
}

Fragment::Fragment( const string &x )
  : id( -1 ), fragment_num( -1 ), final( false ), initialized( true ),
    contents()
{
  fatal_assert( x.size() >= frag_header_len );
  contents = string( x.begin() + frag_header_len, x.end() );

  uint64_t data64;
  uint16_t *data16 = (uint16_t *)x.data();
  memcpy( &data64, x.data(), sizeof( data64 ) );
  id = be64toh( data64 );
  fragment_num = be16toh( data16[ 4 ] );
  final = ( fragment_num & 0x8000 ) >> 15;
  fragment_num &= 0x7FFF;
}

bool FragmentAssembly::add_fragment( Fragment &frag )
{
  /* see if this is a totally new packet */
  if ( current_id != frag.id ) {
    fragments.clear();
    fragments.resize( frag.fragment_num + 1 );
    fragments.at( frag.fragment_num ) = frag;
    fragments_arrived = 1;
    fragments_total = -1; /* unknown */
    current_id = frag.id;
 } else { /* not a new packet */
    /* see if we already have this fragment */
    if ( (fragments.size() > frag.fragment_num)
	 && (fragments.at( frag.fragment_num ).initialized) ) {
      /* make sure new version is same as what we already have */
      assert( fragments.at( frag.fragment_num ) == frag );
    } else {
      if ( (int)fragments.size() < frag.fragment_num + 1 ) {
	fragments.resize( frag.fragment_num + 1 );
      }
      fragments.at( frag.fragment_num ) = frag;
      fragments_arrived++;
    }
  }

  if ( frag.final ) {
    fragments_total = frag.fragment_num + 1;
    assert( (int)fragments.size() <= fragments_total );
    fragments.resize( fragments_total );
  }

  if ( fragments_total != -1 ) {
    assert( fragments_arrived <= fragments_total );
  }

  /* see if we're done */
  return ( fragments_arrived == fragments_total );
}

Instruction FragmentAssembly::get_assembly( void )
{
  assert( fragments_arrived == fragments_total );

  string encoded;

  for ( int i = 0; i < fragments_total; i++ ) {
    assert( fragments.at( i ).initialized );
    encoded += fragments.at( i ).contents;
  }

  Instruction ret;
  fatal_assert( ret.ParseFromString( get_compressor().uncompress_str( encoded ) ) );

  fragments.clear();
  fragments_arrived = 0;
  fragments_total = -1;

  return ret;
}

bool Fragment::operator==( const Fragment &x ) const
{
  return ( id == x.id ) && ( fragment_num == x.fragment_num ) && ( final == x.final )
    && ( initialized == x.initialized ) && ( contents == x.contents );
}

vector<Fragment> Fragmenter::make_fragments( const Instruction &inst, size_t MTU )
{
  MTU -= Fragment::frag_header_len;
  if ( (inst.old_num() != last_instruction.old_num())
       || (inst.new_num() != last_instruction.new_num())
       || (inst.ack_num() != last_instruction.ack_num())
       || (inst.throwaway_num() != last_instruction.throwaway_num())
       || (inst.chaff() != last_instruction.chaff())
       || (inst.protocol_version() != last_instruction.protocol_version())
       || (last_MTU != MTU) ) {
    next_instruction_id++;
  }

  if ( (inst.old_num() == last_instruction.old_num())
       && (inst.new_num() == last_instruction.new_num()) ) {
    assert( inst.diff() == last_instruction.diff() );
  }

  last_instruction = inst;
  last_MTU = MTU;

  string payload = get_compressor().compress_str( inst.SerializeAsString() );
  uint16_t fragment_num = 0;
  vector<Fragment> ret;

  while ( !payload.empty() ) {
    string this_fragment;
    bool final = false;

    if ( payload.size() > MTU ) {
      this_fragment = string( payload.begin(), payload.begin() + MTU );
      payload = string( payload.begin() + MTU, payload.end() );
    } else {
      this_fragment = payload;
      payload.clear();
      final = true;
    }

    ret.push_back( Fragment( next_instruction_id, fragment_num++, final, this_fragment ) );
  }

  return ret;
}
