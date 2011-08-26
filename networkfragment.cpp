#include <endian.h>
#include <assert.h>

#include "networktransport.hpp"
#include "transportinstruction.pb.h"

using namespace Network;
using namespace TransportBuffers;

static string network_order_string( uint16_t host_order )
{
  uint16_t net_int = htobe16( host_order );
  return string( (char *)&net_int, sizeof( net_int ) );
}

string Fragment::tostring( void )
{
  assert( initialized );

  string ret;
  
  ret += network_order_string( id );

  assert( !( fragment_num & 0x8000 ) ); /* effective limit on size of a terminal screen change or buffered user input */
  uint16_t combined_fragment_num = ( final << 15 ) | fragment_num;
  ret += network_order_string( combined_fragment_num );

  assert( ret.size() == frag_header_len );

  ret += contents;

  return ret;
}

Fragment::Fragment( string &x )
  : id( -1 ), fragment_num( -1 ), final( false ), initialized( true ),
    contents( x.begin() + frag_header_len, x.end() )
{
  assert( x.size() >= frag_header_len );

  uint16_t *data16 = (uint16_t *)x.data();
  id = be16toh( data16[ 0 ] );
  fragment_num = be16toh( data16[ 1 ] );
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
  assert( ret.ParseFromString( encoded ) );

  fragments.clear();
  fragments_arrived = 0;
  fragments_total = -1;

  return ret;
}

bool Fragment::operator==( const Fragment &x )
{
  return ( id == x.id ) && ( fragment_num == x.fragment_num ) && ( final == x.final )
    && ( initialized == x.initialized ) && ( contents == x.contents );
}
