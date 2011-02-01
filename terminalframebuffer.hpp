#ifndef TERMINALFB_HPP
#define TERMINALFB_HPP

#include <vector>
#include <deque>

/* Terminal framebuffer */

namespace Terminal {
  class Cell {
  public:
    std::vector<wchar_t> contents;
    char fallback; /* first character is combining character */
    int width;
    std::vector<int> renditions; /* e.g., bold, blinking, etc. */

    Cell();

    Cell( const Cell & );
    Cell & operator=( const Cell & );

    void reset( void );
  };

  class Row {
  public:
    std::vector<Cell> cells;

    Row( size_t s_width );

    void insert_cell( int col );
    void delete_cell( int col );
  };

  class SavedCursor {
  public:
    int cursor_col, cursor_row;
    std::vector<int> renditions;
    /* character set shift state */
    bool auto_wrap_mode;
    bool origin_mode;
    /* state of selective erase */

    SavedCursor();
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

    std::vector<int> renditions;

    SavedCursor save;

  public:
    bool next_print_will_wrap;
    bool origin_mode;
    bool auto_wrap_mode;
    bool insert_mode;
    bool cursor_visible;

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

    std::vector<int> get_tabs( void );

    void set_scrolling_region( int top, int bottom );

    int get_scrolling_region_top_row( void ) { return scrolling_region_top_row; }
    int get_scrolling_region_bottom_row( void ) { return scrolling_region_bottom_row; }

    int limit_top( void );
    int limit_bottom( void );

    void clear_renditions( void ) { renditions.clear(); }
    void add_rendition( int x ) { renditions.push_back( x ); }
    const std::vector<int> get_renditions( void ) { return renditions; }

    void save_cursor( void );
    void restore_cursor( void );
    void clear_saved_cursor( void ) { save = SavedCursor(); }

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

    void apply_renditions_to_current_cell( void );

    void insert_line( int before_row );
    void delete_line( int row );

    void insert_cell( int row, int col );
    void delete_cell( int row, int col );

    void reset( void );
    void soft_reset( void );
  };
}

#endif
