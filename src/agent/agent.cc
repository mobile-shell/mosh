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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/socket.h>

#ifdef SUPPORT_AGENT_FORWARDING
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#else
#undef SUPPORT_AGENT_FORWARDING
#endif
#endif

#include "prng.h"
#include "network.h"
#include "swrite.h"
#include "select.h"
#include "outofband.h"
#include "agent.h"
#include "agent.pb.h"
#include "fatal_assert.h"

using namespace Agent;
using std::string;
using std::map;
using Network::OutOfBand;
using Network::OutOfBandCommunicator;

ProxyAgent::ProxyAgent( bool is_server, bool dummy ) {
  server = is_server;
  ok = false;
  l_sock = -1;
  l_dir = "";
  l_path = "";
  cnt = 0;
  oob_ctl_ptr = NULL;
  comm = NULL;
#ifdef SUPPORT_AGENT_FORWARDING
  if ( dummy ) {
    return;
  }
  if (server) {
    PRNG prng;
    string dir("/tmp/ma-");
    string voc = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;
    for ( i = 0; i < 10; i++ ) {
      dir += voc.substr( prng.uint32() % voc.length(), 1 );
    }
    if ( mkdir( dir.c_str(), 0700 ) != 0 ) {
      return;
    }
    string path(dir + "/");
    for ( i = 0; i < 12; i++ ) {
      path += voc.substr( prng.uint32() % voc.length(), 1 );
    }
    int sock = socket( AF_UNIX, SOCK_STREAM, 0 );
    if ( sock < 0 ) {
      (void) rmdir( dir.c_str() );
      return;
    }
    if ( fcntl( sock, F_SETFD, FD_CLOEXEC ) != 0 ) {
      (void) rmdir( dir.c_str() );
      return;
    }
    struct sockaddr_un sunaddr;
    memset( &sunaddr, 0, sizeof (sunaddr) );
    sunaddr.sun_family = AF_UNIX;
    if ( path.length() >= sizeof (sunaddr.sun_path) ) {
      (void) close( sock );
      (void) rmdir( dir.c_str() );
      return;
    }
    strncpy( sunaddr.sun_path, path.c_str(), sizeof (sunaddr.sun_path) );
    if ( bind( sock, (struct sockaddr *) &sunaddr, sizeof (sunaddr) ) < 0 ) {
      (void) close( sock );
      (void) rmdir( dir.c_str() );
      return;
    }
    if ( listen( sock, AGENT_PROXY_LISTEN_QUEUE_LENGTH ) < 0) {
      (void) close( sock );
      (void) unlink( path.c_str() );
      (void) rmdir( dir.c_str() );
      return;
    }
    l_sock = sock;
    l_path = path;
    l_dir = dir;
  }
  ok = true;
#endif
}

ProxyAgent::~ProxyAgent( void ) {
#ifdef SUPPORT_AGENT_FORWARDING
  shutdown();
#endif
}

void ProxyAgent::close_sessions( void ) {
#ifdef SUPPORT_AGENT_FORWARDING
  map< uint64_t, AgentConnection * >::iterator i = agent_sessions.begin();
  while ( i != agent_sessions.end() ) {
    AgentConnection *ac = i->second;
    agent_sessions.erase( i );
    delete ac;
    i = agent_sessions.begin();
  } 
#endif
}

void ProxyAgent::shutdown( void ) {
#ifdef SUPPORT_AGENT_FORWARDING
  detach_oob();
  if (ok) {
    if ( server && l_sock >= 0 ) {
      (void) close( l_sock );
      (void) unlink( l_path.c_str() );
      (void) rmdir( l_dir.c_str() );
      l_sock = -1;
      l_path = "";
      l_dir = "";
    }
    close_sessions();
    ok = false;
  }
#endif
}

void ProxyAgent::attach_oob(OutOfBand *oob_ctl) {
  detach_oob();
  fatal_assert(oob_ctl != NULL);
  oob_ctl_ptr = oob_ctl;
  comm = oob_ctl_ptr->init(AGENT_FORWARD_OOB_NAME, Network::OOB_MODE_RELIABLE_DATAGRAM, this);
  fatal_assert(comm != NULL);
}

void ProxyAgent::detach_oob(void) {
  if (oob_ctl_ptr != NULL) {
    oob_ctl_ptr->uninit(AGENT_FORWARD_OOB_NAME);
  }
  oob_ctl_ptr = NULL;
}

void ProxyAgent::pre_poll( void ) {
#ifdef SUPPORT_AGENT_FORWARDING
  if ( ! ok ) {
    return;
  }
  Select &sel = Select::get_instance();
  if ( server && l_sock >= 0 ) {
    sel.add_fd( l_sock );
  }
  for ( map< uint64_t, AgentConnection * >::iterator i = agent_sessions.begin(); i != agent_sessions.end(); i++ ) {
    AgentConnection *ac = i->second;
    if ( ac->sock() >= 0 ) {
      sel.add_fd( ac->sock() );
      ac->mark_in_read_set(true);
    } else {
      ac->mark_in_read_set(false);
    }
  }
#endif
}

void ProxyAgent::post_poll( void ) {
#ifdef SUPPORT_AGENT_FORWARDING
  if ( ! ok ) {
    return;
  }
  Select &sel = Select::get_instance();
  // First handle possible incoming data from local sockets
  map< uint64_t, AgentConnection * >::iterator i =  agent_sessions.begin();
  while ( ((! server) || (l_sock >= 0)) && i != agent_sessions.end() ) {
    AgentConnection *ac = i->second;
    if ( (comm == NULL) || (oob_ctl_ptr == NULL) || ac->eof() || (ac->idle_time() > AGENT_IDLE_TIMEOUT) ) {
      agent_sessions.erase( i++ );
      delete ac;
      continue;
    } 

    if ( ac->in_read_set() && sel.read( ac->sock() ) ) {
      while ( true ) {
	string packet = ac->recv_packet();
	if ( ! packet.empty() ) {
	  AgentBuffers::Instruction inst;
	  inst.set_agent_id(ac->s_id);
	  inst.set_agent_data(packet);
	  string pb_packet;
	  fatal_assert(inst.SerializeToString(&pb_packet));
	  comm->send(pb_packet);
	  continue;
	}
	if ( ac->eof() ) {
	  notify_eof(ac->s_id);
	  agent_sessions.erase( i++ );
	  delete ac;
	  break;
	}
	i++;
	break;
      }
    } else {
      i++;
    }
  }
  if ( ! server ) {
    return;
  }
  // Then see if we have mysteriously died in between.
  if ( l_sock < 0 ) {
    return;
  }
  // Then check for new incoming connections.
  if ( sel.read( l_sock ) ) {
    AgentConnection *new_as = get_session();
    if ( new_as != NULL ) {
      agent_sessions[new_as->s_id] = new_as;
    }
  }
#endif
}

void ProxyAgent::post_tick( void ) {
#ifdef SUPPORT_AGENT_FORWARDING
  if ( (! ok) || (comm == NULL) ) {
    return;
  }
  while (comm->readable()) {
    string pb_packet = comm->recv();
    AgentBuffers::Instruction inst;
    fatal_assert( inst.ParseFromString(pb_packet) );
    uint64_t agent_id = inst.agent_id();
    string agent_data = inst.has_agent_data() ? inst.agent_data() : "";
    if (agent_data.empty()) {
      map < uint64_t, AgentConnection* >::iterator i = agent_sessions.find(agent_id);
      if (i != agent_sessions.end()) {
	AgentConnection *ac = i->second;
	agent_sessions.erase( i );
	delete ac;
      }
    } else {
      map < uint64_t, AgentConnection* >::iterator i = agent_sessions.find(agent_id);
      if (i == agent_sessions.end()) {
	AgentConnection *new_as = NULL;
	if (! server) {
	  const char *ap = getenv( "SSH_AUTH_SOCK" );
	  if ( ap != NULL ) {
	    string agent_path(ap);
	    if ( ! agent_path.empty() ) {
	      new_as = new AgentConnection ( agent_path, agent_id, this );
	    }
	  }
	}
	if (new_as == NULL) {
	  notify_eof(agent_id);
	} else {
	  agent_sessions[agent_id] = new_as;
	}
	i = agent_sessions.find(agent_id);
      }
      if (i != agent_sessions.end()) {
	AgentConnection *ac = i->second;
	uint64_t idle = ac->idle_time();
	uint64_t timeout = idle < AGENT_IDLE_TIMEOUT ? (AGENT_IDLE_TIMEOUT - idle) * 1000 : 1;
	if ( swrite_timeout( ac->sock(), timeout, agent_data.c_str(), agent_data.length() ) != 0 ) {
	  agent_sessions.erase( i );
	  delete ac;
	  notify_eof(agent_id);
	}
      }
    }
  }
#endif
}

void ProxyAgent::notify_eof(uint64_t agent_id) {
#ifdef SUPPORT_AGENT_FORWARDING
  if (comm == NULL) {
    return;
  }
  AgentBuffers::Instruction inst;
  inst.set_agent_id(agent_id);
  string pb_packet;
  fatal_assert(inst.SerializeToString(&pb_packet));
  comm->send(pb_packet);
#endif
}


AgentConnection *ProxyAgent::get_session() {
#ifdef SUPPORT_AGENT_FORWARDING
  if ( (! server) || l_sock < 0) {
    return NULL;
  }
  struct sockaddr_un sunaddr; 
  socklen_t slen = sizeof ( sunaddr );
  memset( &sunaddr, 0, slen );
  int sock = accept ( l_sock, (struct sockaddr *)&sunaddr, &slen );
  if ( sock < 0 ) {
    return NULL;
  } 

  if ( (comm == NULL) || (oob_ctl_ptr == NULL) ) {
     (void) close( sock );
     return NULL;
  }

  /* Here we should check that peer effective uid matches with the
     euid of this process.  Skipping however and trusting the file
     system to protect the socket. This would basically catch root
     accessing the socket, but root can change its effective uid to
     match the socket anyways, so it doesn't really help at all. */

  /* If can't set the socket  mode, discard it. */
  if ( fcntl( sock, F_SETFD, FD_CLOEXEC ) != 0 || fcntl( sock, F_SETFL, O_NONBLOCK ) != 0 ) {
     (void) close( sock );
     return NULL;
  }
  return new AgentConnection ( sock, ++cnt, this );
#else
  return NULL;
#endif
}

AgentConnection::AgentConnection(int sock, uint64_t id, ProxyAgent *s_agent_ptr) {
  agent_ptr = s_agent_ptr;
  s_sock = sock;
  s_id = id;
  s_in_read_set = false;
#ifndef SUPPORT_AGENT_FORWARDING
  if (sock >= 0) {
    (void) close( sock );
  }
  s_sock = -1;
#endif
  idle_start = Network::timestamp();
  packet_buf = "";
  packet_len = 0;
}

AgentConnection::AgentConnection(std::string agent_path, uint64_t id, ProxyAgent *s_agent_ptr) {
  agent_ptr = s_agent_ptr;
  s_sock = -1;
  s_id = id;
  s_in_read_set = false;
#ifdef SUPPORT_AGENT_FORWARDING
  int sock = socket( AF_UNIX, SOCK_STREAM, 0 );
  struct sockaddr_un sunaddr;
  memset( &sunaddr, 0, sizeof (sunaddr) );
  sunaddr.sun_family = AF_UNIX;
  if ( agent_path.length() >= sizeof (sunaddr.sun_path) ) {
    (void) close( sock );
    return;
  }
  if ( fcntl( sock, F_SETFD, FD_CLOEXEC ) != 0 ) {
    (void) close( sock );
    return;
  }
  strncpy( sunaddr.sun_path, agent_path.c_str(), sizeof (sunaddr.sun_path) );
  if ( connect(sock, (struct sockaddr *)&sunaddr, sizeof (sunaddr)) < 0 ) {
    (void) close( sock );
    return;
  }
  if ( fcntl( sock, F_SETFL, O_NONBLOCK ) != 0 ) {
    (void) close( sock );
    return;
  }
  s_sock = sock;
#endif
  idle_start = Network::timestamp();
  packet_buf = "";
  packet_len = 0;
}

AgentConnection::~AgentConnection() {
  if ( s_sock >= 0 ) {
    (void) close ( s_sock );
  }
}

uint64_t AgentConnection::idle_time() {
  return (Network::timestamp() - idle_start) / 1000;
}

string AgentConnection::recv_packet() {
#ifdef SUPPORT_AGENT_FORWARDING
  if (eof()) {
    return "";
  }
  ssize_t rv;
  if (packet_len < 1) {
    unsigned char buf[4];
    rv = read( s_sock, buf, 4 );
    if ( (rv < 0) && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) {
      return "";
    }
    if ( rv != 4 ) {
      (void) close(s_sock);
      s_sock = -1;
      return "";
    }
    if ( buf[0] != 0 ) {
      (void) close(s_sock);
      s_sock = -1;
      return "";
    }

    packet_len = (((size_t)buf[1]) << 16) | (((size_t)buf[2]) << 8) | ((size_t)buf[3]);
    if ( packet_len < 1 || packet_len > AGENT_MAXIMUM_PACKET_LENGTH ) {
      (void) close(s_sock);
      s_sock = -1;
      return "";
    }
    packet_buf.append((char *)buf, 4);
    idle_start = Network::timestamp();
  }
  /* read in loop until the entire packet is read or EAGAIN happens */
  do {
    unsigned char buf[1024];
    size_t len = packet_len + 4 - packet_buf.length();
    if (len > sizeof (buf)) {
      len = sizeof (buf);
    }
    rv = read(s_sock, buf, len);
    if ( (rv < 0) && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) { 
      return ""; 
    }
    if ( rv < 1 ) {
      (void) close(s_sock);
      s_sock = -1;
      return "";
    }
    packet_buf.append((char *)buf, rv);    
    idle_start = Network::timestamp();
  } while (packet_buf.length() < (packet_len + 4));
  string packet(packet_buf);
  packet_buf = "";
  packet_len = 0;
  return packet;
#endif
  return "";
}
