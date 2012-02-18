/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdio.h>

#include "terminalframebuffer.h"

using namespace Terminal;

void Cell::reset( int background_color )
{
  contents.clear();
  fallback = false;
  width = 1;
  renditions = Renditions( background_color );
}

DrawState::DrawState( int s_width, int s_height )
  : width( s_width ), height( s_height ),
    cursor_col( 0 ), cursor_row( 0 ),
    combining_char_col( 0 ), combining_char_row( 0 ), tabs( s_width ),
    scrolling_region_top_row( 0 ), scrolling_region_bottom_row( height - 1 ),
    renditions( 0 ), save(),
    next_print_will_wrap( false ), origin_mode( false ), auto_wrap_mode( true ),
    insert_mode( false ), cursor_visible( true ), reverse_video( false ),
    application_mode_cursor_keys( false )
{
  for ( int i = 0; i < width; i++ ) {
    tabs[ i ] = ( (i % 8) == 0 );
  }
}

Framebuffer::Framebuffer( int s_width, int s_height )
  : rows( s_height, Row( s_width, 0 ) ), window_title(), bell_count( 0 ), ds( s_width, s_height )
{
  assert( s_height > 0 );
  assert( s_width > 0 );
}

void Framebuffer::scroll( int N )
{
  if ( N >= 0 ) {
    for ( int i = 0; i < N; i++ ) {
      delete_line( ds.get_scrolling_region_top_row() );
      ds.move_row( -1, true );
    }
  } else {
    N = -N;

    for ( int i = 0; i < N; i++ ) {
      rows.insert( rows.begin() + ds.get_scrolling_region_top_row(), newrow() );
      rows.erase( rows.begin() + ds.get_scrolling_region_bottom_row() + 1 );
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
  if ( cursor_row < limit_top() ) cursor_row = limit_top();
  if ( cursor_row > limit_bottom() ) cursor_row = limit_bottom();
  if ( cursor_col < 0 ) cursor_col = 0;
  if ( cursor_col >= width ) cursor_col = width - 1;
}

void DrawState::move_row( int N, bool relative )
{
  if ( relative ) {
    cursor_row += N;
  } else {
    cursor_row = N + limit_top();
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
  /* don't scroll if outside the scrolling region */
  if ( (ds.get_cursor_row() < ds.get_scrolling_region_top_row())
       || (ds.get_cursor_row() > ds.get_scrolling_region_bottom_row()) ) {
    ds.move_row( rows, true );
    return;
  }

  if ( ds.get_cursor_row() + rows > ds.get_scrolling_region_bottom_row() ) {
    scroll( ds.get_cursor_row() + rows - ds.get_scrolling_region_bottom_row() );
  } else if ( ds.get_cursor_row() + rows < ds.get_scrolling_region_top_row() ) {
    scroll( ds.get_cursor_row() + rows - ds.get_scrolling_region_top_row() );
  }

  ds.move_row( rows, true );
}

Cell *Framebuffer::get_combining_cell( void )
{
  if ( (ds.get_combining_char_col() < 0)
       || (ds.get_combining_char_row() < 0)
       || (ds.get_combining_char_col() >= ds.get_width())
       || (ds.get_combining_char_row() >= ds.get_height()) ) {
    return NULL;
  } /* can happen if a resize came in between */

  return &rows[ ds.get_combining_char_row() ].cells[ ds.get_combining_char_col() ];
}

void DrawState::set_tab( void )
{
  tabs[ cursor_col ] = true;
}

void DrawState::clear_tab( int col )
{
  tabs[ col ] = false;
}

int DrawState::get_next_tab( void )
{
  for ( int i = cursor_col + 1; i < width; i++ ) {
    if ( tabs[ i ] ) {
      return i;
    }
  }
  return -1;
}

void DrawState::set_scrolling_region( int top, int bottom )
{
  if ( height < 1 ) {
    return;
  }

  scrolling_region_top_row = top;
  scrolling_region_bottom_row = bottom;

  if ( scrolling_region_top_row < 0 ) scrolling_region_top_row = 0;
  if ( scrolling_region_bottom_row >= height ) scrolling_region_bottom_row = height - 1;

  if ( scrolling_region_bottom_row < scrolling_region_top_row )
    scrolling_region_bottom_row = scrolling_region_top_row;
  /* real rule requires TWO-line scrolling region */

  if ( origin_mode ) {
    snap_cursor_to_border();
    new_grapheme();
  }
}

int DrawState::limit_top( void )
{
  return origin_mode ? scrolling_region_top_row : 0;
}

int DrawState::limit_bottom( void )
{
  return origin_mode ? scrolling_region_bottom_row : height - 1;
}

std::vector<int> DrawState::get_tabs( void )
{
  std::vector<int> ret;

  for ( int i = 0; i < width; i++ ) {
    if ( tabs[ i ] ) {
      ret.push_back( i );
    }
  }

  return ret;
}

void Framebuffer::apply_renditions_to_current_cell( void )
{
  get_mutable_cell()->renditions = ds.get_renditions();
}

SavedCursor::SavedCursor()
  : cursor_col( 0 ), cursor_row( 0 ),
    renditions( 0 ),
    auto_wrap_mode( true ),
    origin_mode( false )
{}

void DrawState::save_cursor( void )
{
  save.cursor_col = cursor_col;
  save.cursor_row = cursor_row;
  save.renditions = renditions;
  save.auto_wrap_mode = auto_wrap_mode;
  save.origin_mode = origin_mode;
}

void DrawState::restore_cursor( void )
{
  cursor_col = save.cursor_col;
  cursor_row = save.cursor_row;
  renditions = save.renditions;
  auto_wrap_mode = save.auto_wrap_mode;
  origin_mode = save.origin_mode;

  snap_cursor_to_border(); /* we could have resized in between */
  new_grapheme();
}

void Framebuffer::insert_line( int before_row )
{
  if ( (before_row < ds.get_scrolling_region_top_row())
       || (before_row > ds.get_scrolling_region_bottom_row() + 1) ) {
    return;
  }

  rows.insert( rows.begin() + before_row, newrow() );
  rows.erase( rows.begin() + ds.get_scrolling_region_bottom_row() + 1 );
}

void Framebuffer::delete_line( int row )
{
  if ( (row < ds.get_scrolling_region_top_row())
       || (row > ds.get_scrolling_region_bottom_row()) ) {
    return;
  }

  int insertbefore = ds.get_scrolling_region_bottom_row() + 1;
  if ( insertbefore == ds.get_height() ) {
    rows.push_back( newrow() );
  } else {
    rows.insert( rows.begin() + insertbefore, newrow() );
  }

  rows.erase( rows.begin() + row );
}

void Row::insert_cell( int col, int background_color )
{
  cells.insert( cells.begin() + col, Cell( background_color ) );
  cells.pop_back();
}

void Row::delete_cell( int col, int background_color )
{
  cells.push_back( Cell( background_color ) );
  cells.erase( cells.begin() + col );
}

void Framebuffer::insert_cell( int row, int col )
{
  rows[ row ].insert_cell( col, ds.get_background_rendition() );
}

void Framebuffer::delete_cell( int row, int col )
{
  rows[ row ].delete_cell( col, ds.get_background_rendition() );
}

void Framebuffer::reset( void )
{
  int width = ds.get_width(), height = ds.get_height();
  ds = DrawState( width, height );
  rows = std::deque<Row>( height, newrow() );
  window_title.clear();
  /* do not reset bell_count */
}

void Framebuffer::soft_reset( void )
{
  ds.insert_mode = false;
  ds.origin_mode = false;
  ds.cursor_visible = true; /* per xterm and gnome-terminal */
  ds.application_mode_cursor_keys = false;
  ds.set_scrolling_region( 0, ds.get_height() - 1 );
  ds.add_rendition( 0 );
  ds.clear_saved_cursor();
}

void Framebuffer::resize( int s_width, int s_height )
{
  assert( s_width > 0 );
  assert( s_height > 0 );

  rows.resize( s_height, newrow() );

  for ( std::deque<Row>::iterator i = rows.begin();
	i != rows.end();
	i++ ) {
    (*i).cells.resize( s_width, Cell( ds.get_background_rendition() ) );
  }

  ds.resize( s_width, s_height );
}

void DrawState::resize( int s_width, int s_height )
{
  if ( (width != s_width)
       || (height != s_height) ) {
    /* reset entire scrolling region on any resize */
    /* xterm and rxvt-unicode do this. gnome-terminal only
       resets scrolling region if it has to become smaller in resize */
    scrolling_region_top_row = 0;
    scrolling_region_bottom_row = s_height - 1;
  }

  width = s_width;
  height = s_height;

  snap_cursor_to_border();

  tabs.resize( width );

  /* saved cursor will be snapped to border on restore */

  /* invalidate combining char cell if necessary */
  if ( (combining_char_col >= width)
       || (combining_char_row >= height) ) {
    combining_char_col = combining_char_row = -1;
  }
}

Renditions::Renditions( int s_background )
  : bold( false ), underlined( false ), blink( false ),
    inverse( false ), invisible( false ), foreground_color( 0 ),
    background_color( s_background )
{}

void Renditions::set_rendition( int num )
{
  if ( num == 0 ) {
    bold = underlined = blink = inverse = invisible = false;
    foreground_color = background_color = 0;
    return;
  }

  if ( (30 <= num) && (num <= 39) ) { /* foreground color */
    foreground_color = num;
    return;
  } else if ( (40 <= num) && (num <= 49) ) { /* background color */
    background_color = num;
    return;
  }

  switch ( num ) {
  case 1: case 22: bold = (num == 1); break;
  case 4: case 24: underlined = (num == 4); break;
  case 5: case 25: blink = (num == 5); break;
  case 7: case 27: inverse = (num == 7); break;
  case 8: case 28: invisible = (num == 8); break;
  }
}

std::string Renditions::sgr( void ) const
{
  std::string ret;

  ret.append( "\033[0" );
  if ( bold ) ret.append( ";1" );
  if ( underlined ) ret.append( ";4" );
  if ( blink ) ret.append( ";5" );
  if ( inverse ) ret.append( ";7" );
  if ( invisible ) ret.append( ";8" );
  if ( foreground_color ) {
    char col[ 8 ];
    snprintf( col, 8, ";%d", foreground_color );
    ret.append( col );
  }
  if ( background_color ) {
    char col[ 8 ];
    snprintf( col, 8, ";%d", background_color );
    ret.append( col );
  }
  ret.append( "m" );

  return ret;
}

void Row::reset( int background_color )
{
  for ( std::vector<Cell>::iterator i = cells.begin();
	i != cells.end();
	i++ ) {
    i->reset( background_color );
  }
}

void Framebuffer::prefix_window_title( const std::deque<wchar_t> &s )
{
  for ( auto i = s.rbegin(); i != s.rend(); i++ ) {
    window_title.push_front( *i );
  }
}

wchar_t Cell::debug_contents( void ) const
{
  if ( contents.empty() ) {
    return '_';
  } else {
    return contents.front();
  }
}
