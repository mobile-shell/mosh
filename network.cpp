#include <assert.h>

#include "network.hpp"

using namespace std;
using namespace Network;

template <class Payload>
Connection<Payload>::Packet::Packet( int64_t s_seq, int64_t s_ack, Packet *s_previous, Payload s_state )
  : seq( s_seq ), ack( s_ack ), previous( s_previous ), state( s_state )
{

}

template <class Payload>
Connection<Payload>::Packet::Packet( string wire )
{
  assert( wire.length() >= 32 );

  seq = be64toh( (uint64_t) *wire_c );
  reference_seq = be64toh( (uint64_t) *( wire_c + 8 ) );

  tag = string( wire.begin() + 16, wire.begin() + 32 );

  ack = be64toh( (uint64_t) *( wire_c + 16 ) );
}

template <class Payload>
Connection<Payload>::Connection( const char *ip, const char *port, bool server )
{

}
