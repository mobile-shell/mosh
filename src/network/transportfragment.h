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

#ifndef TRANSPORT_FRAGMENT_HPP
#define TRANSPORT_FRAGMENT_HPP

#include <stdint.h>
#include <vector>
#include <string>

#include "transportinstruction.pb.h"

using std::vector;
using std::string;
using namespace TransportBuffers;

namespace Network {
  class Fragment
  {
  public:
    static const size_t frag_header_len = sizeof( uint64_t ) + sizeof( uint16_t );

    uint64_t id;
    uint16_t fragment_num;
    bool final;

    bool initialized;

    string contents;

    Fragment()
      : id( -1 ), fragment_num( -1 ), final( false ), initialized( false ), contents()
    {}

    Fragment( uint64_t s_id, uint16_t s_fragment_num, bool s_final, const string & s_contents )
      : id( s_id ), fragment_num( s_fragment_num ), final( s_final ), initialized( true ),
	contents( s_contents )
    {}

    Fragment( const string &x );

    string tostring( void );

    bool operator==( const Fragment &x ) const;
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
    size_t last_MTU;

  public:
    Fragmenter() : next_instruction_id( 0 ), last_instruction(), last_MTU( -1 )
    {
      last_instruction.set_old_num( -1 );
      last_instruction.set_new_num( -1 );
    }
    vector<Fragment> make_fragments( const Instruction &inst, size_t MTU );
    uint64_t last_ack_sent( void ) const { return last_instruction.ack_num(); }
  };
  
}

#endif
