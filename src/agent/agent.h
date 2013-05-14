/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    SSH Agent forwarding for Mosh
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

#ifndef AGENT_HPP
#define AGENT_HPP

#include <string>
#include <map>

#include "outofband.h"

#define AGENT_MAXIMUM_PACKET_LENGTH 32768 // Not counting the length field.
#define AGENT_MAXIMUM_OUTPUT_BUFFER_LENGTH (AGENT_MAXIMUM_PACKET_LENGTH * 4) // Counting all data
#define AGENT_IDLE_TIMEOUT 30 // In seconds. Must be enforced by the caller.
#define AGENT_PROXY_LISTEN_QUEUE_LENGTH 4
#define AGENT_FORWARD_OOB_NAME "ssh-agent-forward"

namespace Agent {

  class ProxyAgent;

  class AgentConnection
  {
  private:
    bool s_in_read_set;
    int s_sock;
    uint64_t s_id;
    uint64_t idle_start;
    string packet_buf;
    size_t packet_len;
    AgentConnection(int sock, uint64_t id, ProxyAgent *s_agent_ptr);
    AgentConnection(std::string agent_path, uint64_t id, ProxyAgent *s_agent_ptr);
    ~AgentConnection();
    int sock() { return s_sock; }
    bool eof() { return (s_sock < 0); }
    std::string recv_packet();
    uint64_t idle_time();
    void mark_in_read_set(bool val) { s_in_read_set = val; }
    bool in_read_set( void ) { return s_in_read_set; }
    ProxyAgent *agent_ptr;

  public:
    friend class ProxyAgent;
  };

  class ProxyAgent : public Network::OutOfBandPlugin
  {
  private:
    void detach_oob(void);
    Network::OutOfBandCommunicator *comm;
    Network::OutOfBand *oob_ctl_ptr;
    Network::OutOfBand *oob( void ) { return oob_ctl_ptr; }
    void notify_eof(uint64_t agent_id);
    AgentConnection *get_session();
    bool server;
    bool ok;
    int l_sock;
    string l_dir;
    string l_path;
    uint64_t cnt;
    std::map< uint64_t, AgentConnection * > agent_sessions;

  public:
    // Required by parent class
    bool active( void ) { return ok && ((! server) || (l_sock >= 0)); }
    void pre_poll( void );
    void post_poll( void );
    void post_tick( void );
    void close_sessions( void );
    void shutdown( void );
    void attach_oob(Network::OutOfBand *oob_ctl);

    // Class specific stuff
    ProxyAgent( bool is_server, bool dummy = false );
    ~ProxyAgent( void );
    std::string listener_path( void ) { if ( ok && server && l_sock >= 0 ) return l_path; return ""; }

    friend class AgentConnection;
  };

}

#endif
