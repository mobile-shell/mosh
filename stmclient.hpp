#ifndef STM_CLIENT_HPP
#define STM_CLIENT_HPP

#include <termios.h>
#include <string>

class STMClient {
private:
  std::string ip;
  int port;
  std::string key;

  struct termios saved_termios, raw_termios;

public:
  STMClient( const char *s_ip, int s_port, const char *s_key )
    : ip( s_ip ), port( s_port ), key( s_key ),
      saved_termios(), raw_termios()
  {}

  void init( void );
  void shutdown( void );
  void main( void );
};

#endif
