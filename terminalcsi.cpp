#include <unistd.h>

#include "terminal.hpp"

using namespace Terminal;

static void clearline( Framebuffer *fb, int row, int start, int end )
{
  for ( int col = start; col <= end; col++ ) {
    fb->get_cell( row, col )->reset();
  }
}

void Emulator::CSI_EL( void )
{
  /* default: active position to end of line, inclusive */
  int start = fb.ds.get_cursor_col(), end = fb.ds.get_width() - 1;

  if ( as.params == "1" ) { /* start of screen to active position, inclusive */
    start = 0;
    end = fb.ds.get_cursor_col();
  } else if ( as.params == "2" ) { /* all of line */
    start = 0;
  }

  clearline( &fb, -1, start, end );
}

void Emulator::CSI_ED( void ) {
  if ( as.params == "1" ) { /* start of screen to active position, inclusive */
    for ( int y = 0; y < fb.ds.get_cursor_row(); y++ ) {
      clearline( &fb, y, 0, fb.ds.get_width() - 1 );
    }
    clearline( &fb, -1, 0, fb.ds.get_cursor_col() );
  } else if ( as.params == "2" ) { /* entire screen */
    for ( int y = 0; y < fb.ds.get_height(); y++ ) {
      clearline( &fb, y, 0, fb.ds.get_width() - 1 );
    }
  } else { /* active position to end of screen, inclusive */
    clearline( &fb, -1, fb.ds.get_cursor_col(), fb.ds.get_width() - 1 );
    for ( int y = fb.ds.get_cursor_row() + 1; y < fb.ds.get_height(); y++ ) {
      clearline( &fb, y, 0, fb.ds.get_width() - 1 );
    }
  }
}

void Emulator::CSI_cursormove( void )
{
  int num = as.getparam( 0, 1 );

  switch ( as.dispatch_chars[ 0 ] ) {
  case 'A':
    fb.ds.move_row( -num, true );
    break;
  case 'B':
    fb.ds.move_row( num, true );
    break;
  case 'C':
    fb.ds.move_col( num, true );
    break;
  case 'D':
    fb.ds.move_col( -num, true );
    break;
  case 'H':
  case 'f':
    int x = as.getparam( 0, 1 );
    int y = as.getparam( 1, 1 );
    fb.ds.move_row( x - 1 );
    fb.ds.move_col( y - 1 );
  }
}

void Emulator::CSI_DA( void )
{
  terminal_to_host.append( "\033[?1;0c" );
}

void Emulator::Esc_DECALN( void )
{
  for ( int y = 0; y < fb.ds.get_height(); y++ ) {
    for ( int x = 0; x < fb.ds.get_width(); x++ ) {
      fb.get_cell( y, x )->reset();
      fb.get_cell( y, x )->contents.push_back( L'E' );
    }
  }
}
