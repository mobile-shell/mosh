#ifndef TERMINALFB_HPP
#define TERMINALFB_HPP

#include <vector>
#include <deque>

/* Terminal framebuffer */

namespace Terminal {
  class Cell {
  public:
    Cell *overlapping_cell;
    std::vector<wchar_t> contents;
    std::vector<Cell *> overlapped_cells;
    char fallback; /* first character is combining character */
    int width;

    Cell();

    Cell( const Cell & );
    Cell & operator=( const Cell & );

    void reset( void );
  };

  class Row {
  public:
    std::vector<Cell> cells;

    Row( size_t s_width );
  };

  class DrawState {
  private:
    int width, height;

    void new_grapheme( void );
    void snap_cursor_to_border( void );

    int cursor_col, cursor_row;
    int combining_char_col, combining_char_row;

    std::vector<bool> tabs;

    int scrolling_region_top_row, scrolling_region_bottom_row;

  public:
    bool next_print_will_wrap;
    bool origin_mode;
    bool auto_wrap_mode;

    /* bold, etc. */

    void move_row( int N, bool relative = false );
    void move_col( int N, bool relative = false, bool implicit = false );

    int get_cursor_col( void ) { return cursor_col; }
    int get_cursor_row( void ) { return cursor_row; }
    int get_combining_char_col( void ) { return combining_char_col; }
    int get_combining_char_row( void ) { return combining_char_row; }
    int get_width( void ) { return width; }
    int get_height( void ) { return height; }

    void set_tab( void );
    void clear_tab( int col );
    int get_next_tab( void );

    void set_scrolling_region( int top, int bottom );

    int limit_top( void );
    int limit_bottom( void );

    DrawState( int s_width, int s_height );
  };

  class Framebuffer {
  private:
    std::deque<Row> rows;

    void scroll( int N );

  public:
    Framebuffer( int s_width, int s_height );
    DrawState ds;

    void move_rows_autoscroll( int rows );

    Cell *get_cell( void );
    Cell *get_cell( int row, int col );
    Cell *get_combining_cell( void );

    void claim_overlap( int row, int col );
  };
}

#endif
