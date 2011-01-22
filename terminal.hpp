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

    void reset( void );
};

  class Row {
  public:
    std::vector<Cell> cells;

    Row( size_t s_width );
  };

  class Emulator {
    friend void Parser::Print::act_on_terminal( Emulator * );
    friend void Parser::Execute::act_on_terminal( Emulator * );
    friend void Parser::Clear::act_on_terminal( Emulator * );
    friend void Parser::Param::act_on_terminal( Emulator * );
    friend void Parser::Collect::act_on_terminal( Emulator * );
    friend void Parser::CSI_Dispatch::act_on_terminal( Emulator * );
    friend void Parser::Esc_Dispatch::act_on_terminal( Emulator * );

  private:
    Parser::UTF8Parser parser;

    int width, height;
    int cursor_col, cursor_row;
    int combining_char_col, combining_char_row;

    std::deque<Row> rows;
    std::string params;
    std::string dispatch_chars;

    std::string terminal_to_host;
    std::string errors;

    /* action methods */
    void print( Parser::Print *act );
    void execute( Parser::Execute *act );
    void param( Parser::Param *act );
    void collect( Parser::Collect *act );
    void clear( Parser::Clear *act );
    void CSI_dispatch( Parser::CSI_Dispatch *act );
    void Esc_dispatch( Parser::Esc_Dispatch *act );

    void scroll( int N );
    void autoscroll( void );
    void newgrapheme( void );

    void parse_params( void );
    std::vector<int> parsed_params;
    int getparam( size_t N, int defaultval );

    /* CSI and Escape methods */
    void CSI_EL( void );
    void CSI_ED( void );
    void CSI_cursormove( void );
    void CSI_DA( void );
    void Esc_DECALN( void );

  public:
    Emulator( size_t s_width, size_t s_height );
    ~Emulator();

    std::string input( char c, int debug_fd );

    void resize( size_t s_width, size_t s_height );

    size_t get_width( void ) { return width; }
    size_t get_height( void ) { return height; }

    void debug_printout( int fd );
  };
}

#endif
