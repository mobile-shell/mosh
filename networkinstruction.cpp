#include <endian.h>
#include <assert.h>

#include "networktransport.hpp"

using namespace Network;

static string network_order_string( uint64_t host_order )
{
  uint64_t net_int = htobe64( host_order );
  return string( (char *)&net_int, sizeof( net_int ) );
}

static string network_order_string( uint16_t host_order )
{
  uint16_t net_int = htobe16( host_order );
  return string( (char *)&net_int, sizeof( net_int ) );
}

string Instruction::tostring( void )
{
  string ret;
  
  ret += network_order_string( old_num );
  ret += network_order_string( new_num );
  ret += network_order_string( ack_num );
  ret += network_order_string( throwaway_num );
  ret += network_order_string( fragment_num );

  assert( ret.size() == inst_header_len );

  ret += diff;

  return ret;
}

Instruction::Instruction( string &x )
  : old_num( -1 ), new_num( -1 ), ack_num( -1 ), throwaway_num( -1 ), fragment_num( -1 ), diff()
{
  assert( x.size() >= inst_header_len );
  uint64_t *data = (uint64_t *)x.data();
  uint16_t *data16 = (uint16_t *)x.data();
  old_num = be64toh( data[ 0 ] );
  new_num = be64toh( data[ 1 ] );
  ack_num = be64toh( data[ 2 ] );
  throwaway_num = be64toh( data[ 3 ] );
  fragment_num = be16toh( data16[ 16 ] );

  diff = string( x.begin() + inst_header_len, x.end() );
}

bool FragmentAssembly::same_template( Instruction &a, Instruction &b )
{
  return ( a.old_num == b.old_num ) && ( a.new_num == b.new_num ) && ( a.ack_num == b.ack_num )
    && ( a.throwaway_num == b.throwaway_num );
}

bool FragmentAssembly::add_fragment( Instruction &inst )
{
  /* decode fragment num */
  bool last_fragment = inst.fragment_num > 32767;
  uint16_t real_fragment_num = inst.fragment_num;
  if ( last_fragment ) {
    real_fragment_num -= 32768;
  }

  /* see if this is a totally new packet */
  if ( !same_template( inst, current_template ) ) {
    fragments.clear();
    current_template = inst;
    fragments.resize( real_fragment_num + 1 );
    fragments[ real_fragment_num ] = inst;
    fragments_arrived = 1;
    fragments_total = -1;
  } else { /* not a new packet */
    /* see if we already have this fragment */
    if ( fragments[ real_fragment_num ].old_num != uint64_t(-1) ) {
      assert( fragments[ real_fragment_num ] == inst );
    } else {
      if ( fragments_total == -1 ) {
	fragments.resize( real_fragment_num + 1 );
      }
      fragments.at( real_fragment_num ) = inst;
      fragments_arrived++;
    }
  }

  if ( last_fragment ) {
    fragments_total = real_fragment_num + 1;
    fragments.resize( fragments_total );
  }

  /* see if we're done */
  return ( fragments_arrived == fragments_total );
}

Instruction FragmentAssembly::get_assembly( void )
{
  assert( fragments_arrived == fragments_total );

  Instruction ret( current_template );
  ret.diff = "";

  for ( int i = 0; i < fragments_total; i++ ) {
    ret.diff += fragments[ i ].diff;
  }

  return ret;
}

bool Instruction::operator==( const Instruction &x )
{
  return ( old_num == x.old_num ) && ( new_num == x.new_num )
    && ( ack_num == x.ack_num ) && ( throwaway_num == x.throwaway_num )
    && ( fragment_num == x.fragment_num ) && ( diff == x.diff );
}
