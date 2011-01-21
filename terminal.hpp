#ifndef TERMINAL_CPP
#define TERMINAL_CPP

#include <wchar.h>
#include <stdio.h>
#include <vector>
#include <deque>

#include "parser.hpp"

namespace Terminal {
  class Cell {
  public:
    Cell *overlapping_cell;
    std::vector<wchar_t> contents;
    std::vector<Cell *> overlapped_cells;

    Cell();
  
    Cell( const Cell & );
    Cell & operator=( const Cell & );
};

  class Row {
  public:
    std::vector<Cell> cells;

    Row( size_t s_width );
  };

  class Emulator {
    friend void Parser::Print::act_on_terminal( Emulator * );

  private:
    Parser::UTF8Parser parser;

    size_t width, height;
    size_t cursor_col, cursor_row;
    size_t combining_char_col, combining_char_row;

    std::deque<Row> rows;

    void print( Parser::Print *act );

    void scroll( int N );

    void newgrapheme( void );

  public:
    Emulator( size_t s_width, size_t s_height );
    ~Emulator();

    void input( char c );

    void resize( size_t s_width, size_t s_height );

    size_t get_width( void ) { return width; }
    size_t get_height( void ) { return height; }

    void debug_printout( FILE *f );
  };
}

#endif
