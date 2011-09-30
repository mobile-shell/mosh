#ifndef STM_CLIENT_HPP
#define STM_CLIENT_HPP

#include <sys/ioctl.h>
#include <termios.h>
#include <string>

#include "completeterminal.hpp"
#include "networktransport.hpp"
#include "user.hpp"

class STMClient {
private:
  std::string ip;
  int port;
  std::string key;

  struct termios saved_termios, raw_termios;

  int winch_fd, shutdown_signal_fd;
  struct winsize window_size;

  Network::Transport< Network::UserStream, Terminal::Complete > *network;
  uint64_t last_remote_num;

  void main_init( void );
  bool process_network_input( void );
  bool process_user_input( int fd );
  bool process_resize( void );

public:
  STMClient( const char *s_ip, int s_port, const char *s_key )
    : ip( s_ip ), port( s_port ), key( s_key ),
      saved_termios(), raw_termios(),
      winch_fd(), shutdown_signal_fd(),
      window_size(),
      network( NULL ),
      last_remote_num( -1 )
  {}

  void init( void );
  void shutdown( void );
  void main( void );

  ~STMClient()
  {
    if ( network != NULL ) {
      delete network;
    }
  }

  /* unused */
  STMClient( const STMClient & );
  STMClient & operator=( const STMClient & );
};

#endif
