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

  class Framebuffer {
  public:
    int width, height;
    int cursor_col, cursor_row;
    int combining_char_col, combining_char_row;

    std::deque<Row> rows;

    void scroll( int N );
    void autoscroll( void );
    void newgrapheme( void );

    Framebuffer( int s_width, int s_height );
  };
}

#endif
