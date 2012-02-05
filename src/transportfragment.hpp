#ifndef TRANSPORT_FRAGMENT_HPP
#define TRANSPORT_FRAGMENT_HPP

#include <stdint.h>
#include <vector>
#include <string>

#include "transportinstruction.pb.h"

using namespace std;
using namespace TransportBuffers;

namespace Network {
  static const int HEADER_LEN = 66;

  class Fragment
  {
  private:
    static const size_t frag_header_len = sizeof( uint64_t ) + sizeof( uint16_t );

  public:
    uint64_t id;
    uint16_t fragment_num;
    bool final;

    bool initialized;

    string contents;

    Fragment()
      : id( -1 ), fragment_num( -1 ), final( false ), initialized( false ), contents()
    {}

    Fragment( uint64_t s_id, uint16_t s_fragment_num, bool s_final, string s_contents )
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
    uint64_t current_id;
    int fragments_arrived, fragments_total;

  public:
    FragmentAssembly() : fragments(), current_id( -1 ), fragments_arrived( 0 ), fragments_total( -1 ) {}
    bool add_fragment( Fragment &inst );
    Instruction get_assembly( void );
  };

  class Fragmenter
  {
  private:
    uint64_t next_instruction_id;
    Instruction last_instruction;
    int last_MTU;

  public:
    Fragmenter() : next_instruction_id( 0 ), last_instruction(), last_MTU( -1 )
    {
      last_instruction.set_old_num( -1 );
      last_instruction.set_new_num( -1 );
    }
    vector<Fragment> make_fragments( Instruction &inst, int MTU );
    uint64_t last_ack_sent( void ) const { return last_instruction.ack_num(); }
  };
  
}

#endif
