#include <termios.h>
#include <unistd.h>
#include <poll.h>

#include "keystroke.hpp"
#include "networktransport.hpp"

bool readable( int fd )
{
  struct pollfd my_pollfd;
  my_pollfd.fd = fd;
  my_pollfd.events = POLLIN;

  int num = poll( &my_pollfd, 1, 0 );
  if ( num < 0 ) {
    perror( "poll" );
    exit( 1 );
  }
  
  return my_pollfd.revents & POLLIN;
}

int main( int argc, char *argv[] )
{
  bool server = true;
  char *key;
  char *ip;
  int port;

  KeyStroke me, remote;

  Network::Transport<KeyStroke, KeyStroke> *n;

  try {
    if ( argc > 1 ) {
      server = false;
      /* client */
      
      key = argv[ 1 ];
      ip = argv[ 2 ];
      port = atoi( argv[ 3 ] );
      
      n = new Network::Transport<KeyStroke, KeyStroke>( me, remote, key, ip, port );
    } else {
      n = new Network::Transport<KeyStroke, KeyStroke>( me, remote );
    }
  } catch ( CryptoException e ) {
    fprintf( stderr, "Fatal error: %s\n", e.text.c_str() );
    exit( 1 );
  }

  fprintf( stderr, "Port bound is %d, key is %s\n", n->port(), n->get_key().c_str() );

  if ( server ) {
    while ( true ) {
      try {
	n->recv();
	n->tick();
	fprintf( stderr, "Num: %d. Contents: ",
		 (int)n->get_remote_state_num() );
	for ( size_t i = 0; i < n->get_remote_state().user_bytes.size(); i++ ) {
	  fprintf( stderr, "%c", n->get_remote_state().user_bytes[ i ] );
	}
	fprintf( stderr, "\n" );
      } catch ( CryptoException e ) {
	fprintf( stderr, "Cryptographic error: %s\n", e.text.c_str() );
      }
    }
  } else {
    struct termios saved_termios;
    struct termios the_termios;

    if ( tcgetattr( STDIN_FILENO, &the_termios ) < 0 ) {
      perror( "tcgetattr" );
      exit( 1 );
    }

    saved_termios = the_termios;

    cfmakeraw( &the_termios );

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &the_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }

    while( true ) {
      char x = getchar();
      
      n->get_current_state().key_hit( x );
      
      try {
	if ( readable( n->fd() ) ) {
	  n->recv();
	}
	n->tick();
      } catch ( Network::NetworkException e ) {
	fprintf( stderr, "%s: %s\r\n", e.function.c_str(), strerror( e.the_errno ) );
	break;
      } catch ( CryptoException e ) {
	fprintf( stderr, "Cryptographic error: %s\n", e.text.c_str() );
      }
    }

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }    
  }
}
