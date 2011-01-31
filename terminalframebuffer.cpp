#include <assert.h>

#include "terminalframebuffer.hpp"

using namespace Terminal;

Cell::Cell()
  : overlapping_cell( NULL ),
    contents(),
    overlapped_cells(),
    fallback( false ),
    width( 1 )
{}

Cell::Cell( const Cell &x )
  : overlapping_cell( x.overlapping_cell ),
    contents( x.contents ),
    overlapped_cells( x.overlapped_cells ),
    fallback( x.fallback ),
    width( 1 )
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
  width = 1;

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

DrawState::DrawState( int s_width, int s_height )
  : width( s_width ), height( s_height ),
    cursor_col( 0 ), cursor_row( 0 ),
    combining_char_col( 0 ), combining_char_row( 0 ),
    next_print_will_wrap( false )
{}

Framebuffer::Framebuffer( int s_width, int s_height )
  : rows( s_height, Row( s_width ) ), ds( s_width, s_height )
{}

void Framebuffer::scroll( int N )
{
  if ( N >= 0 ) {
    for ( int i = 0; i < N; i++ ) {
      rows.pop_front();
      rows.push_back( Row( ds.get_width() ) );
      ds.move_row( -1, true );
    }
  } else {
    N = -N;

    for ( int i = 0; i < N; i++ ) {
      rows.pop_back();
      rows.push_front( Row( ds.get_width() ) );
      ds.move_row( 1, true );
    }
  }
}

void DrawState::new_grapheme( void )
{
  combining_char_col = cursor_col;
  combining_char_row = cursor_row;  
}

void DrawState::snap_cursor_to_border( void )
{
  if ( cursor_row < 0 ) cursor_row = 0;
  if ( cursor_row >= height ) cursor_row = height - 1;
  if ( cursor_col < 0 ) cursor_col = 0;
  if ( cursor_col >= width ) cursor_col = width - 1;
}

void DrawState::move_row( int N, bool relative )
{
  if ( relative ) {
    cursor_row += N;
  } else {
    cursor_row = N;
  }

  snap_cursor_to_border();
  new_grapheme();
  next_print_will_wrap = false;
}

void DrawState::move_col( int N, bool relative, bool implicit )
{
  if ( implicit ) {
    new_grapheme();
  }

  if ( relative ) {
    cursor_col += N;
  } else {
    cursor_col = N;
  }

  if ( implicit && (cursor_col >= width) ) {
    next_print_will_wrap = true;
  }

  snap_cursor_to_border();
  if ( !implicit ) {
    new_grapheme();
    next_print_will_wrap = false;
  }
}

void Framebuffer::move_rows_autoscroll( int rows )
{
  if ( ds.get_cursor_row() + rows >= ds.get_height() ) {
    scroll( ds.get_height() - ds.get_cursor_row() - rows + 1 );
  } else if ( ds.get_cursor_row() + rows < 0 ) {
    scroll( ds.get_cursor_row() + rows );
  }

  ds.move_row( rows, true );
}

Cell *Framebuffer::get_cell( void )
{
  if ( (ds.get_width() == 0) || (ds.get_height() == 0) ) {
    return NULL;
  }

  return &rows[ ds.get_cursor_row() ].cells[ ds.get_cursor_col() ];
}

Cell *Framebuffer::get_cell( int row, int col )
{
  if ( row == -1 ) row = ds.get_cursor_row();
  if ( col == -1 ) col = ds.get_cursor_col();

  return &rows[ row ].cells[ col ];
}

Cell *Framebuffer::get_combining_cell( void )
{
  return &rows[ ds.get_combining_char_row() ].cells[ ds.get_combining_char_col() ];
}

void Framebuffer::claim_overlap( int row, int col )
{
  Cell *the_cell = &rows[ row ].cells[ col ];

  for ( int i = col + 1; i < col + the_cell->width; i++ ) {
    if ( i < ds.get_width() ) {
      Cell *next_cell = get_cell( row, i );
      next_cell->reset();
      the_cell->overlapped_cells.push_back( next_cell );
      next_cell->overlapping_cell = the_cell;
    }
  }
}
