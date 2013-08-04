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

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#ifndef TERMINALFB_HPP
#define TERMINALFB_HPP

#include <vector>
#include <deque>
#include <string>
#include <list>
#include <assert.h>

/* Terminal framebuffer */

namespace Terminal {
  class Renditions {
  public:
    bool bold, italic, underlined, blink, inverse, invisible;
    int foreground_color;
    int background_color;

    Renditions( int s_background );
    void set_foreground_color( int num );
    void set_background_color( int num );
    void set_rendition( int num );
    std::string sgr( void ) const;

    void posterize( void );

    bool operator==( const Renditions &x ) const
    {
      return (bold == x.bold) && (italic == x.italic) && (underlined == x.underlined)
	&& (blink == x.blink) && (inverse == x.inverse)
	&& (invisible == x.invisible) && (foreground_color == x.foreground_color)
	&& (background_color == x.background_color);
    }
  };

  class Cell {
  public:
    std::vector<wchar_t> contents;
    char fallback; /* first character is combining character */
    int width;
    Renditions renditions;
    bool wrap; /* if last cell, wrap to next line */

    Cell( int background_color )
      : contents(),
	fallback( false ),
	width( 1 ),
	renditions( background_color ),
	wrap( false )
    {}

    Cell() /* default constructor required by C++11 STL */
      : contents(),
	fallback( false ),
	width( 1 ),
	renditions( 0 ),
	wrap( false )
    {
      assert( false );
    }

    void reset( int background_color );

    bool operator==( const Cell &x ) const
    {
      return ( (contents == x.contents)
	       && (fallback == x.fallback)
	       && (width == x.width)
	       && (renditions == x.renditions)
	       && (wrap == x.wrap) );
    }

    wchar_t debug_contents( void ) const;

    bool is_blank( void ) const
    {
      return ( contents.empty()
	       || ( (contents.size() == 1) && ( (contents.front() == 0x20)
						|| (contents.front() == 0xA0) ) ) );
    }

    bool contents_match ( const Cell &other ) const
    {
      return ( is_blank() && other.is_blank() )
             || ( contents == other.contents );
    }

    bool compare( const Cell &other ) const;
  };

  class Row {
  public:
    typedef std::vector<Cell> cells_type;
    cells_type cells;

    Row( size_t s_width, int background_color )
      : cells( s_width, Cell( background_color ) )
    {}

    Row() /* default constructor required by C++11 STL */
      : cells( 1, Cell() )
    {
      assert( false );
    }

    void insert_cell( int col, int background_color );
    void delete_cell( int col, int background_color );

    void reset( int background_color );

    bool operator==( const Row &x ) const
    {
      return ( cells == x.cells );
    }

    bool get_wrap( void ) const { return cells.back().wrap; }
    void set_wrap( bool w ) { cells.back().wrap = w; }
  };

  class SavedCursor {
  public:
    int cursor_col, cursor_row;
    Renditions renditions;
    /* not implemented: character set shift state */
    bool auto_wrap_mode;
    bool origin_mode;
    /* not implemented: state of selective erase */

    SavedCursor();
  };

  class DrawState {
  private:
    int width, height;

    void new_grapheme( void );
    void snap_cursor_to_border( void );

    int cursor_col, cursor_row;
    int combining_char_col, combining_char_row;

    bool default_tabs;
    std::vector<bool> tabs;

    void reinitialize_tabs( unsigned int start );

    int scrolling_region_top_row, scrolling_region_bottom_row;

    Renditions renditions;

    SavedCursor save;

  public:
    bool next_print_will_wrap;
    bool origin_mode;
    bool auto_wrap_mode;
    bool insert_mode;
    bool cursor_visible;
    bool reverse_video;
    bool bracketed_paste;
    bool vt100_mouse;
    bool xterm_mouse;
    bool xterm_extended_mouse; // aka SGR
    bool xterm_utf8_mouse;

    bool application_mode_cursor_keys;

    /* bold, etc. */

    void move_row( int N, bool relative = false );
    void move_col( int N, bool relative = false, bool implicit = false );

    int get_cursor_col( void ) const { return cursor_col; }
    int get_cursor_row( void ) const { return cursor_row; }
    int get_combining_char_col( void ) const { return combining_char_col; }
    int get_combining_char_row( void ) const { return combining_char_row; }
    int get_width( void ) const { return width; }
    int get_height( void ) const { return height; }

    void set_tab( void );
    void clear_tab( int col );
    void clear_default_tabs( void ) { default_tabs = false; }
    /* Default tabs can't be restored without resetting the draw state. */
    int get_next_tab( void );

    void set_scrolling_region( int top, int bottom );

    int get_scrolling_region_top_row( void ) const { return scrolling_region_top_row; }
    int get_scrolling_region_bottom_row( void ) const { return scrolling_region_bottom_row; }

    int limit_top( void );
    int limit_bottom( void );

    void set_foreground_color( int x ) { renditions.set_foreground_color( x ); }
    void set_background_color( int x ) { renditions.set_background_color( x ); }
    void add_rendition( int x ) { renditions.set_rendition( x ); }
    Renditions get_renditions( void ) const { return renditions; }
    int get_background_rendition( void ) const { return renditions.background_color; }

    void save_cursor( void );
    void restore_cursor( void );
    void clear_saved_cursor( void ) { save = SavedCursor(); }

    void resize( int s_width, int s_height );

    DrawState( int s_width, int s_height );

    bool operator==( const DrawState &x ) const
    {
      /* only compare fields that affect display */
      return ( width == x.width ) && ( height == x.height ) && ( cursor_col == x.cursor_col )
	&& ( cursor_row == x.cursor_row ) && ( cursor_visible == x.cursor_visible ) &&
	( reverse_video == x.reverse_video ) && ( renditions == x.renditions ) &&
  ( bracketed_paste == x.bracketed_paste ) && ( vt100_mouse == x.vt100_mouse ) &&
  ( xterm_mouse == x.xterm_mouse ) && ( xterm_extended_mouse == x.xterm_extended_mouse ) && 
  ( xterm_utf8_mouse == x.xterm_utf8_mouse );
    }
  };

  class Framebuffer {
  private:
    typedef std::deque<Row> rows_type;
    rows_type rows;
    std::deque<wchar_t> icon_name;
    std::deque<wchar_t> window_title;
    unsigned int bell_count;
    bool title_initialized; /* true if the window title has been set via an OSC */

    Row newrow( void ) { return Row( ds.get_width(), ds.get_background_rendition() ); }

  public:
    Framebuffer( int s_width, int s_height );
    DrawState ds;

    void scroll( int N );
    void move_rows_autoscroll( int rows );

    const Row *get_row( int row ) const
    {
      if ( row == -1 ) row = ds.get_cursor_row();

      return &rows[ row ];
    }

    inline const Cell *get_cell( void ) const
    {
      return &rows[ ds.get_cursor_row() ].cells[ ds.get_cursor_col() ];
    }

    inline const Cell *get_cell( int row, int col ) const
    {
      if ( row == -1 ) row = ds.get_cursor_row();
      if ( col == -1 ) col = ds.get_cursor_col();

      return &rows[ row ].cells[ col ];
    }

    Row *get_mutable_row( int row )
    {
      if ( row == -1 ) row = ds.get_cursor_row();

      return &rows[ row ];
    }

    inline Cell *get_mutable_cell( void )
    {
      return &rows[ ds.get_cursor_row() ].cells[ ds.get_cursor_col() ];
    }

    inline Cell *get_mutable_cell( int row, int col )
    {
      if ( row == -1 ) row = ds.get_cursor_row();
      if ( col == -1 ) col = ds.get_cursor_col();

      return &rows[ row ].cells[ col ];
    }

    Cell *get_combining_cell( void );

    void apply_renditions_to_current_cell( void );

    void insert_line( int before_row );
    void delete_line( int row );

    void insert_cell( int row, int col );
    void delete_cell( int row, int col );

    void reset( void );
    void soft_reset( void );

    void set_title_initialized( void ) { title_initialized = true; }
    bool is_title_initialized( void ) const { return title_initialized; }
    void set_icon_name( const std::deque<wchar_t> &s ) { icon_name = s; }
    void set_window_title( const std::deque<wchar_t> &s ) { window_title = s; }
    const std::deque<wchar_t> & get_icon_name( void ) const { return icon_name; }
    const std::deque<wchar_t> & get_window_title( void ) const { return window_title; }

    void prefix_window_title( const std::deque<wchar_t> &s );

    void resize( int s_width, int s_height );

    void reset_cell( Cell *c ) { c->reset( ds.get_background_rendition() ); }
    void reset_row( Row *r ) { r->reset( ds.get_background_rendition() ); }

    void posterize( void );

    void ring_bell( void ) { bell_count++; }
    unsigned int get_bell_count( void ) const { return bell_count; }

    bool operator==( const Framebuffer &x ) const
    {
      return ( rows == x.rows ) && ( window_title == x.window_title ) && ( bell_count == x.bell_count ) && ( ds == x.ds );
    }
  };
}

#endif
