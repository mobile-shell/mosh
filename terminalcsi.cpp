#include <unistd.h>

#include "terminal.hpp"

using namespace Terminal;

void Emulator::CSI_EL( void )
{
  if ( params == "1" ) { /* start of screen to active position, inclusive */
    for ( int x = 0; x <= cursor_col; x++ ) {
      rows[ cursor_row ].cells[ x ].contents.clear();
    }
  } else if ( params == "2" ) { /* all of line */
    rows[ cursor_row ] = Row( width );
  } else { /* active position to end of line, inclusive */
    for ( int x = cursor_col; x < width; x++ ) {
      rows[ cursor_row ].cells[ x ].contents.clear();
    }
  }
}

void Emulator::CSI_cursormove( void )
{
  parse_params();
  int num = getparam( 0, 1 );

  switch ( dispatch_chars[ 0 ] ) {
  case 'A':
    cursor_row -= num;
    break;
  case 'B':
    cursor_row += num;
    break;
  case 'C':
    cursor_col += num;
    break;
  case 'D':
    cursor_col -= num;
    break;
  case 'H':
    int x = getparam( 0, 1 );
    int y = getparam( 1, 1 );
    cursor_row = x - 1;
    cursor_col = y - 1;
  }

  if ( cursor_row < 0 ) cursor_row = 0;
  if ( cursor_row >= height ) cursor_row = height - 1;
  if ( cursor_col < 0 ) cursor_col = 0;
  if ( cursor_col >= width ) cursor_col = width - 1;

  newgrapheme();
}
