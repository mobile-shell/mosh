#ifndef STM_CLIENT_HPP
#define STM_CLIENT_HPP

#include <sys/ioctl.h>
#include <termios.h>
#include <string>

#include "completeterminal.hpp"
#include "networktransport.hpp"
#include "user.hpp"
#include "terminaloverlay.hpp"

class STMClient {
private:
  std::string ip;
  int port;
  std::string key;

  struct termios saved_termios, raw_termios;

  int winch_fd, shutdown_signal_fd;
  struct winsize window_size;

  Terminal::Framebuffer *local_framebuffer;
  Overlay::OverlayManager overlays;
  Network::Transport< Network::UserStream, Terminal::Complete > *network;

  bool repaint_requested, quit_sequence_started;

  void main_init( void );
  bool process_network_input( void );
  bool process_user_input( int fd );
  bool process_resize( void );

  void output_new_frame( void );

public:
  STMClient( const char *s_ip, int s_port, const char *s_key )
    : ip( s_ip ), port( s_port ), key( s_key ),
      saved_termios(), raw_termios(),
      winch_fd(), shutdown_signal_fd(),
      window_size(),
      local_framebuffer( NULL ),
      overlays(),
      network( NULL ),
      repaint_requested( false ),
      quit_sequence_started( false )
  {}

  void init( void );
  void shutdown( void );
  void main( void );

  ~STMClient()
  {
    if ( local_framebuffer != NULL ) {
      delete local_framebuffer;
    }

    if ( network != NULL ) {
      delete network;
    }
  }

  /* unused */
  STMClient( const STMClient & );
  STMClient & operator=( const STMClient & );
};

#endif
