#ifndef TRANSPORT_FRAGMENT_HPP
#define TRANSPORT_FRAGMENT_HPP

#include <stdint.h>
#include <vector>
#include <string>

#include "transportinstruction.pb.h"

using namespace std;
using namespace TransportBuffers;

namespace Network {
  class Fragment
  {
  private:
    static const size_t frag_header_len = 2 * sizeof( uint16_t );

  public:
    uint16_t id;
    uint16_t fragment_num;
    bool final;

    bool initialized;

    string contents;

    Fragment()
      : id( -1 ), fragment_num( -1 ), final( false ), initialized( false ), contents()
    {}

    Fragment( uint16_t s_id, uint16_t s_fragment_num, bool s_final, string s_contents )
      : id( s_id ), fragment_num( s_fragment_num ), final( s_final ), initialized( true ),
	contents( s_contents )
    {}

    Fragment( string &x );

    string tostring( void );

    bool operator==( const Fragment &x );
  };

  class FragmentAssembly
  {
  private:
    vector<Fragment> fragments;
    uint16_t current_id;
    int fragments_arrived, fragments_total;

  public:
    FragmentAssembly() : fragments(), current_id( -1 ), fragments_arrived( 0 ), fragments_total( -1 ) {}
    bool add_fragment( Fragment &inst );
    Instruction get_assembly( void );
  };
}

#endif
