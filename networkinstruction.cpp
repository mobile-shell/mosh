#include <endian.h>
#include <assert.h>

#include "networktransport.hpp"

using namespace Network;

static string network_order_string( uint64_t host_order )
{
  uint64_t net_int = htobe64( host_order );
  return string( (char *)&net_int, sizeof( net_int ) );
}

string Instruction::tostring( void )
{
  string ret;
  
  ret += network_order_string( old_num );
  ret += network_order_string( new_num );
  ret += network_order_string( ack_num );
  ret += network_order_string( throwaway_num );
  ret += diff;

  return ret;
}

Instruction::Instruction( string &x )
  : old_num( -1 ), new_num( -1 ), ack_num( -1 ), throwaway_num( -1 ), diff()
{
  assert( x.size() >= 4 * sizeof( uint64_t ) );
  uint64_t *data = (uint64_t *)x.data();
  old_num = be64toh( data[ 0 ] );
  new_num = be64toh( data[ 1 ] );
  ack_num = be64toh( data[ 2 ] );
  throwaway_num = be64toh( data[ 3 ] );

  diff = string( x.begin() + 4 * sizeof( uint64_t ), x.end() );
}
