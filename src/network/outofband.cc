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

#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "fatal_assert.h"

#include "outofband.h"
#include "oob.pb.h"

#include <limits.h>

using namespace Network;
using namespace OutOfBandBuffers;
using namespace std;


OutOfBand::OutOfBand() {
  seq_num_out = 0;
  ack_num_out = 0;
}

OutOfBandCommunicator *OutOfBand::init(string name, OutOfBandMode mode, OutOfBandPlugin *plugin) {
  map < string, OutOfBandCommunicator * >::iterator i = comms.find(name);
  if (i != comms.end()) {
    return NULL;
  }
  OutOfBandCommunicator *comm = new OutOfBandCommunicator(mode, name, this, plugin);
  comms[name] = comm;
  return comm;
}

void OutOfBand::pre_poll( void ) {
  map < string, OutOfBandCommunicator * >::iterator i = comms.begin();
  while (i != comms.end()) {
    OutOfBandCommunicator *comm = (i++)->second;
    if (comm->plugin_ptr->active()) {
      comm->plugin_ptr->pre_poll();
    }
  }
}

void OutOfBand::post_poll( void ) {
  map < string, OutOfBandCommunicator * >::iterator i = comms.begin();
  while (i != comms.end()) {
    OutOfBandCommunicator *comm = (i++)->second;
    if (comm->plugin_ptr->active()) {
      comm->plugin_ptr->post_poll();
    }
  }
}

void OutOfBand::post_tick( void ) {
  map < string, OutOfBandCommunicator * >::iterator i = comms.begin();
  while (i != comms.end()) {
    OutOfBandCommunicator *comm = (i++)->second;
    if (comm->plugin_ptr->active()) {
      comm->plugin_ptr->post_tick();
    }
  }
}

void OutOfBand::close_sessions( void ) {
  map < string, OutOfBandCommunicator * >::iterator i = comms.begin();
  while (i != comms.end()) {
    OutOfBandCommunicator *comm = (i++)->second;
    comm->plugin_ptr->close_sessions();
  }
}

void OutOfBand::shutdown( void ) {
  map < string, OutOfBandCommunicator * >::iterator i = comms.begin();
  while (i != comms.end()) {
    OutOfBandCommunicator *comm = (i++)->second;
    comm->plugin_ptr->shutdown();
  }
}

void OutOfBand::uninit(string name) {
  map < string, OutOfBandCommunicator * >::iterator i = comms.find(name);
  if (i == comms.end()) {
    return;
  }
  OutOfBandCommunicator *comm = i->second;
  comms.erase(i);
  delete comm;
}

void OutOfBand::uninit(OutOfBandCommunicator *comm) {
  uninit(comm->name);
}

void OutOfBand::uninit(void) {
  map < string, OutOfBandCommunicator * >::iterator i;
  while ((i = comms.begin()) != comms.end()) {
    OutOfBandCommunicator *comm = i->second;
    comms.erase(i);
    delete comm;
  }
}

void OutOfBand::input(string data) {
  Instruction inst;
  fatal_assert( inst.ParseFromString(data) );
  if (inst.has_ack_num()) {
    uint64_t ack_num = inst.ack_num();
    if (ack_num != 0) {
      list < OutOfBandBuffers::Instruction >::iterator i = reliable_instruction_out_sent.begin();
      while (i != reliable_instruction_out_sent.end()) {
	fatal_assert((*i).has_seq_num());
	if ((*i).seq_num() <= ack_num) {
	  i = reliable_instruction_out_sent.erase(i);
	  continue;
	}
	break;
      }
    }
  }

  bool ack = false;

  if (inst.has_payload_type() && inst.has_payload_data()) {
    string payload_type = inst.payload_type();
    string payload_data = inst.payload_data();
    uint64_t seq_num = inst.has_seq_num() ? inst.seq_num() : 0;
    uint64_t oob_mode = inst.has_oob_mode() ? inst.oob_mode() : 0;
    OutOfBandCommunicator *comm = NULL;
    map < string, OutOfBandCommunicator * >::iterator i = comms.find(payload_type);
    if (i != comms.end()) {
      comm = i->second;
      fatal_assert(oob_mode == (uint64_t)comm->mode);
    }
    if (seq_num == 0) {
      fatal_assert(oob_mode == (uint64_t)OOB_MODE_DATAGRAM);
      if (comm != NULL) {
	comm->datagram_queue.push(payload_data);
      }
    } else {
      fatal_assert(oob_mode == (uint64_t)OOB_MODE_STREAM || oob_mode == (uint64_t)OOB_MODE_RELIABLE_DATAGRAM);
      if (seq_num == next_seq_num(ack_num_out)) {
	if (comm != NULL) {
	  switch (comm->mode) {
	  case OOB_MODE_STREAM:
	    comm->stream_buf += payload_data;
	    break;
	  case OOB_MODE_RELIABLE_DATAGRAM:
	    comm->datagram_queue.push(payload_data);
	    break;
	  default:
	    //NOTREACHED
	    fatal_assert(comm->mode == OOB_MODE_STREAM || comm->mode == OOB_MODE_RELIABLE_DATAGRAM);
	  }
	}
	ack_num_out = seq_num;
      }
      ack = true;
    }
  }
  
  if (ack && (! has_unsent_output())) {
    Instruction inst;
    datagram_instruction_out.push(inst);
  }
}

bool OutOfBand::has_output(void) {
  return (! (datagram_instruction_out.empty() && reliable_instruction_out_sent.empty() && reliable_instruction_out_unsent.empty()));
}

bool OutOfBand::has_unsent_output(void) {
  return (! (datagram_instruction_out.empty() && reliable_instruction_out_unsent.empty()));
}

string OutOfBand::output(void) {
  string rv("");
  if (! datagram_instruction_out.empty()) {
    Instruction inst = datagram_instruction_out.front();
    if (ack_num_out != 0) {
      inst.set_ack_num(ack_num_out);
    }
    fatal_assert(inst.SerializeToString(&rv));
    datagram_instruction_out.pop();
    return rv;
  }
  if (! reliable_instruction_out_sent.empty()) {
    Instruction inst = reliable_instruction_out_sent.front();
    if (ack_num_out != 0) {
      inst.set_ack_num(ack_num_out);
    }
    fatal_assert(inst.SerializeToString(&rv));
    return rv;
  }
  if (! reliable_instruction_out_unsent.empty()) {
    Instruction inst = reliable_instruction_out_unsent.front();
    reliable_instruction_out_sent.push_back(inst);
    reliable_instruction_out_unsent.pop_front();
    if (ack_num_out != 0) {
      inst.set_ack_num(ack_num_out);
    }
    fatal_assert(inst.SerializeToString(&rv));
    return rv;
  }
  return "";
}

OutOfBandCommunicator::OutOfBandCommunicator(OutOfBandMode oob_mode, string oob_name, OutOfBand *oob_ctl, OutOfBandPlugin *plugin) {
  mode = oob_mode;
  name = oob_name;
  oob_ctl_ptr = oob_ctl;
  plugin_ptr = plugin;
  stream_buf = "";
}

void OutOfBandCommunicator::send(string data) {
  Instruction inst;
  if (oob()->ack_num_out != 0) {
    inst.set_ack_num(oob()->ack_num_out);
  }
  inst.set_payload_type(name);
  inst.set_payload_data(data);
  inst.set_oob_mode((uint64_t)mode);
  switch (mode) {
  case OOB_MODE_STREAM:
  case OOB_MODE_RELIABLE_DATAGRAM:
    inst.set_seq_num(oob()->increment_seq_num_out());
    oob()->reliable_instruction_out_unsent.push_back(inst);
    break;
    //FALLTHROUGH
  case OOB_MODE_DATAGRAM:
    oob()->datagram_instruction_out.push(inst);
  }
}

bool OutOfBandCommunicator::readable(void) {
  switch (mode) {
  case OOB_MODE_STREAM:
    return (! stream_buf.empty());
  case OOB_MODE_DATAGRAM:
  case OOB_MODE_RELIABLE_DATAGRAM:
    return (! datagram_queue.empty());
  }
  //NOTREACHED
  return false;
}

string OutOfBandCommunicator::recv(void) {
  string rv("");
  switch (mode) {
  case OOB_MODE_STREAM:
    if (stream_buf.empty()) {
      return rv;
    }
    rv = stream_buf;
    stream_buf = "";
    return rv;
  case OOB_MODE_RELIABLE_DATAGRAM:
  case OOB_MODE_DATAGRAM:
    if (datagram_queue.empty()) {
      return rv;
    }
    rv = datagram_queue.front();
    datagram_queue.pop();
    return rv;
  }
  //NOTREACHED
  return "";
}

string OutOfBandCommunicator::read(size_t len) {
  fatal_assert(mode == OOB_MODE_STREAM);
  if (stream_buf.length() < len) {
    return "";
  }
  string rv = stream_buf.substr(0, len);
  stream_buf = stream_buf.substr(len);
  return rv;
}
