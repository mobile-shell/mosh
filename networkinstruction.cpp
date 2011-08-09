#include <endian.h>

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
  ret += diff;

  return ret;
}
