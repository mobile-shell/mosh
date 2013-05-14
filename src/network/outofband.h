/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    Out of band protocol extension for Mosh
    Copyright 2013 Timo J. Rinne

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


#ifndef OUT_OF_BAND_HPP
#define OUT_OF_BAND_HPP

#include <string>
#include <queue>
#include <list>
#include <map>

#include "oob.pb.h"

using std::string;
using std::queue;
using std::list;
using std::map;

namespace Network {

  enum OutOfBandMode { OOB_MODE_STREAM = 1, OOB_MODE_DATAGRAM = 2, OOB_MODE_RELIABLE_DATAGRAM = 3 };

  class OutOfBand;
  class OutOfBandPlugin;
  class OutOfBandCommunicator;

  class OutOfBandCommunicator
  {
  private:
    OutOfBandMode mode;
    string name;
    string stream_buf;
    queue < string > datagram_queue;
    OutOfBandPlugin *plugin_ptr;
    OutOfBand *oob_ctl_ptr;
    OutOfBand *oob(void) { return oob_ctl_ptr; }
    OutOfBandCommunicator(OutOfBandMode oob_mode, string oob_name, OutOfBand *oob_ctl, OutOfBandPlugin *plugin);

  public:
    void send(string data);
    bool readable(void);
    string recv(void);
    string read(size_t len);

    friend class OutOfBand;
  };

  class OutOfBand
  {
  private:
    map < string, OutOfBandCommunicator * > comms;
    queue < OutOfBandBuffers::Instruction > datagram_instruction_out;
    list < OutOfBandBuffers::Instruction > reliable_instruction_out_sent;
    list < OutOfBandBuffers::Instruction > reliable_instruction_out_unsent;
    uint64_t seq_num_out;
    uint64_t ack_num_out;
    uint64_t next_seq_num(uint64_t sn) { sn++; if (sn == 0) sn++; return sn; }
    uint64_t increment_seq_num_out(void) { seq_num_out = next_seq_num(seq_num_out); return seq_num_out; }

  public:
    OutOfBand();
    ~OutOfBand() { uninit(); }

    void pre_poll( void );
    void post_poll( void );
    void post_tick( void );
    void close_sessions( void );
    void shutdown( void );

    OutOfBandCommunicator *init(string name, OutOfBandMode mode, OutOfBandPlugin *plugin);
    void uninit(string name);
    void uninit(OutOfBandCommunicator *comm);
    void uninit(void);
    bool has_output(void);
    bool has_unsent_output(void);
    // input and output are to be called from transport code only
    void input(string data);
    string output(void);

    friend class OutOfBandCommunicator;
  };

  class OutOfBandPlugin
  {
  public:
    virtual bool active( void ) = 0;
    virtual void pre_poll( void ) = 0;
    virtual void post_poll( void ) = 0;
    virtual void post_tick( void ) = 0;
    virtual void close_sessions( void ) = 0;
    virtual void shutdown( void ) = 0;
    virtual void attach_oob(Network::OutOfBand *oob_ctl) = 0;

    friend class OutOfBand;
  };

}

#endif
