#include <assert.h>

#include "terminal.hpp"

using namespace Terminal;

Cell::Cell()
  : overlapping_cell( NULL ),
    contents(),
    overlapped_cells()
{}

Cell::Cell( const Cell &x )
  : overlapping_cell( x.overlapping_cell ),
    contents( x.contents ),
    overlapped_cells( x.overlapped_cells )
{}

Cell & Cell::operator=( const Cell &x )
{
  overlapping_cell = x.overlapping_cell;
  contents = x.contents;
  overlapped_cells = x.overlapped_cells;

  return *this;
}

Row::Row( size_t s_width )
  : cells( s_width )
{}

Emulator::Emulator( size_t s_width, size_t s_height )
  : parser(),
    width( s_width ), height( s_height ),
    cursor_col( 0 ), cursor_row( 0 ),
    combining_char_col( 0 ), combining_char_row( 0 ),
    rows( height, Row( width ) )
{

}

Emulator::~Emulator()
{

}

void Emulator::input( char c )
{
  std::vector<Parser::Action *> vec = parser.input( c );

  for ( std::vector<Parser::Action *>::iterator i = vec.begin();
	i != vec.end();
	i++ ) {
    Parser::Action *act = *i;

    act->act_on_terminal( this );

    delete act;
  }
}

void Emulator::scroll( int N )
{
  if ( N == 0 ) {
    return;
  } else if ( N > 0 ) {
    for ( int i = 0; i < N; i++ ) {
      rows.pop_front();
      rows.push_back( Row( width ) );
      cursor_row--;
      combining_char_row--;
    }
  }
}

void Emulator::newgrapheme( void )
{
  combining_char_col = cursor_col;
  combining_char_row = cursor_row;  
}

void Emulator::autoscroll( void )
{
  if ( cursor_row >= height ) { /* scroll */
    scroll( cursor_row - height + 1 );
  }
}

void Emulator::execute( Parser::Execute *act )
{
  assert( act->char_present );

  switch ( act->ch ) {
  case 0x0a: /* LF */
    cursor_row++;
    autoscroll();
    break;

  case 0x0d: /* CR */
    cursor_col = 0;
    break;
    
  case 0x08: /* BS */
    if ( cursor_col > 0 ) {
      cursor_col--;
      rows[ cursor_row ].cells[ cursor_col ].contents.clear();
    }
    break;
  }
}

void Emulator::print( Parser::Print *act )
{
  assert( act->char_present );

  if ( (width == 0) || (height == 0) ) {
    return;
  }

  assert( cursor_row < height ); /* must be on screen */
  assert( cursor_row <= width ); /* one off is ok */

  int chwidth = act->ch == L'\0' ? -1 : wcwidth( act->ch );

  switch ( chwidth ) {
  case 1: /* normal character */
  case 2: /* wide character */
    if ( cursor_col >= width ) { /* wrap */
      cursor_col = 0;
      cursor_row++;
    }

    autoscroll();

    rows[ cursor_row ].cells[ cursor_col ].contents.clear();
    rows[ cursor_row ].cells[ cursor_col ].contents.push_back( act->ch );
    newgrapheme();
    cursor_col++;
    break;
  default:
    break;
  }
}

void Emulator::debug_printout( FILE *f )
{
  fprintf( f, "\033[H\033[2J" );

  for ( size_t y = 0; y < height; y++ ) {
    for ( size_t x = 0; x < width; x++ ) {
      fprintf( f, "\033[%d;%dH", y + 1, x + 1 );
      Cell *cell = &rows[ y ].cells[ x ];
      for ( std::vector<wchar_t>::iterator i = cell->contents.begin();
	    i != cell->contents.end();
	    i++ ) {
	fprintf( f, "%lc", *i );
      }
    }
  }

  fprintf( f, "\033[%d;%dH", cursor_row + 1, cursor_col + 1 );

  fflush( NULL );
}
