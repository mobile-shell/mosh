#include "terminalframebuffer.hpp"

using namespace Terminal;

Cell::Cell()
  : overlapping_cell( NULL ),
    contents(),
    overlapped_cells(),
    fallback( false )
{}

Cell::Cell( const Cell &x )
  : overlapping_cell( x.overlapping_cell ),
    contents( x.contents ),
    overlapped_cells( x.overlapped_cells ),
    fallback( x.fallback )
{}

Cell & Cell::operator=( const Cell &x )
{
  overlapping_cell = x.overlapping_cell;
  contents = x.contents;
  overlapped_cells = x.overlapped_cells;
  fallback = x.fallback;

  return *this;
}

Row::Row( size_t s_width )
  : cells( s_width )
{}

void Cell::reset( void )
{
  contents.clear();
  fallback = false;

  if ( overlapping_cell ) {
    assert( overlapped_cells.size() == 0 );
  } else {
    for ( std::vector<Cell *>::iterator i = overlapped_cells.begin();
	  i != overlapped_cells.end();
	  i++ ) {
      (*i)->overlapping_cell = NULL;
      (*i)->reset();
    }
    overlapped_cells.clear();
  }
}

Framebuffer::Framebuffer( int s_width, int s_height )
  : width( s_width ), height( s_height ),
    cursor_col( 0 ), cursor_row( 0 ),
    combining_char_col( 0 ), combining_char_row( 0 ),
    rows( height, Row( width ) )
{}

void Framebuffer::scroll( int N )
{
  assert( N >= 0 );

  for ( int i = 0; i < N; i++ ) {
    rows.pop_front();
    rows.push_back( Row( width ) );
    cursor_row--;
    combining_char_row--;
  }
}

void Framebuffer::newgrapheme( void )
{
  combining_char_col = cursor_col;
  combining_char_row = cursor_row;  
}

void Framebuffer::autoscroll( void )
{
  if ( cursor_row >= height ) { /* scroll */
    scroll( cursor_row - height + 1 );
  }
}

