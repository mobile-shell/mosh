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

void Emulator::print( Parser::Print *act )
{
  fprintf( stderr, "print (%lc)!\r\n", act->ch );
}
