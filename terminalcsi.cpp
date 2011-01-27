#include <unistd.h>

#include "terminal.hpp"

using namespace Terminal;

void Emulator::CSI_EL( void )
{
  if ( as.params == "1" ) { /* start of screen to active position, inclusive */
    for ( int x = 0; x <= fb.cursor_col; x++ ) {
      if ( x < fb.width ) {
	fb.rows[ fb.cursor_row ].cells[ x ].reset();
      }
    }
  } else if ( as.params == "2" ) { /* all of line */
    fb.rows[ fb.cursor_row ] = Row( fb.width );
  } else { /* active position to end of line, inclusive */
    for ( int x = fb.cursor_col; x < fb.width; x++ ) {
      fb.rows[ fb.cursor_row ].cells[ x ].reset();
    }
  }
}

void Emulator::CSI_ED( void ) {
  if ( as.params == "1" ) { /* start of screen to active position, inclusive */
    for ( int y = 0; y < fb.cursor_row; y++ ) {
      for ( int x = 0; x < fb.width; x++ ) {
	fb.rows[ y ].cells[ x ].reset();
      }
    }
    for ( int x = 0; x <= fb.cursor_col; x++ ) {
      if ( x < fb.width ) {
	fb.rows[ fb.cursor_row ].cells[ x ].reset();
      }
    }
  } else if ( as.params == "2" ) { /* entire screen */
    for ( int y = 0; y < fb.height; y++ ) {
      for ( int x = 0; x < fb.width; x++ ) {
	fb.rows[ y ].cells[ x ].reset();
      }
    }
  } else { /* active position to end of screen, inclusive */
    for ( int x = fb.cursor_col; x < fb.width; x++ ) {
      fb.rows[ fb.cursor_row ].cells[ x ].reset();
    }
    for ( int y = fb.cursor_row + 1; y < fb.height; y++ ) {
      for ( int x = 0; x < fb.width; x++ ) {
	fb.rows[ y ].cells[ x ].reset();
      }
    }
  }
}

void Emulator::CSI_cursormove( void )
{
  int num = as.getparam( 0, 1 );

  switch ( as.dispatch_chars[ 0 ] ) {
  case 'A':
    fb.cursor_row -= num;
    break;
  case 'B':
    fb.cursor_row += num;
    break;
  case 'C':
    fb.cursor_col += num;
    break;
  case 'D':
    fb.cursor_col -= num;
    break;
  case 'H':
  case 'f':
    int x = as.getparam( 0, 1 );
    int y = as.getparam( 1, 1 );
    fb.cursor_row = x - 1;
    fb.cursor_col = y - 1;
  }

  if ( fb.cursor_row < 0 ) fb.cursor_row = 0;
  if ( fb.cursor_row >= fb.height ) fb.cursor_row = fb.height - 1;
  if ( fb.cursor_col < 0 ) fb.cursor_col = 0;
  if ( fb.cursor_col >= fb.width ) fb.cursor_col = fb.width - 1;

  fb.newgrapheme();
}

void Emulator::CSI_DA( void )
{
  terminal_to_host.append( "\033[?1;0c" );
}

void Emulator::Esc_DECALN( void )
{
  for ( int y = 0; y < fb.height; y++ ) {
    for ( int x = 0; x < fb.width; x++ ) {
      fb.rows[ y ].cells[ x ].reset();
      fb.rows[ y ].cells[ x ].contents.push_back( L'E' );
    }
  }
}
