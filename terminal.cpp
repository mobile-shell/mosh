#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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
    rows( height, Row( width ) ),
    params(), dispatch_chars(), errors(), parsed_params()
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

void Emulator::scroll( int N )
{
  if ( N == 0 ) {
    return;
  } else if ( N > 0 ) {
    for ( int i = 0; i < N; i++ ) {
      rows.pop_front();
      rows.push_back( Row( width ) );
      cursor_row--;
      combining_char_row--;
    }
  }
}

void Emulator::newgrapheme( void )
{
  combining_char_col = cursor_col;
  combining_char_row = cursor_row;  
}

void Emulator::autoscroll( void )
{
  if ( cursor_row >= height ) { /* scroll */
    scroll( cursor_row - height + 1 );
  }
}

void Emulator::execute( Parser::Execute *act )
{
  assert( act->char_present );

  switch ( act->ch ) {
  case 0x0a: /* LF */
    cursor_row++;
    autoscroll();
    break;

  case 0x0d: /* CR */
    cursor_col = 0;
    break;
    
  case 0x08: /* BS */
    if ( cursor_col > 0 ) {
      cursor_col--;
      newgrapheme(); /* this is not xterm's behavior */
    }
    break;
  }
}

void Emulator::print( Parser::Print *act )
{
  assert( act->char_present );

  if ( (width == 0) || (height == 0) ) {
    return;
  }

  assert( cursor_row < height ); /* must be on screen */
  assert( cursor_row <= width ); /* one off is ok */

  int chwidth = act->ch == L'\0' ? -1 : wcwidth( act->ch );

  Cell *this_cell;

  switch ( chwidth ) {
  case 1: /* normal character */
  case 2: /* wide character */
    if ( cursor_col >= width ) { /* wrap */
      cursor_col = 0;
      cursor_row++;
    }

    autoscroll();

    this_cell =  &rows[ cursor_row ].cells[ cursor_col ];
    this_cell->contents.clear();
    this_cell->contents.push_back( act->ch );
    for ( std::vector<Cell *>::iterator i
	    = this_cell->overlapped_cells.begin();
	  i != this_cell->overlapped_cells.end();
	  i++ ) {
      **i = Cell();
    }
    this_cell->overlapped_cells.clear();

    newgrapheme();

    if ( cursor_col < width - 1 ) {
      Cell *next_cell = &rows[ cursor_row ].cells[ cursor_col + 1 ];
      if ( chwidth == 2 ) {
	this_cell->overlapped_cells.push_back( next_cell );
	next_cell->overlapping_cell = this_cell;
      } else {
	next_cell->overlapping_cell = NULL;
      }
    }

    cursor_col += chwidth;
    break;
  case 0: /* combining character */
    if ( rows[ combining_char_row ].cells[ combining_char_col ].contents.size() < 16 ) { /* seems like a reasonable limit on combining character */
      rows[ combining_char_row ].cells[ combining_char_col ].contents.push_back( act->ch );
    }
  case -1: /* unprintable character */
    break;
  default:
    assert( false );
  }
}

void Emulator::debug_printout( FILE *f )
{
  fprintf( f, "\033[H\033[2J" );

  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      fprintf( f, "\033[%d;%dH", y + 1, x + 1 );
      Cell *cell = &rows[ y ].cells[ x ];
      if ( cell->overlapping_cell ) continue;
      for ( std::vector<wchar_t>::iterator i = cell->contents.begin();
	    i != cell->contents.end();
	    i++ ) {
	fprintf( f, "%lc", *i );
      }
    }
  }

  fprintf( f, "\033[%d;%dH", cursor_row + 1, cursor_col + 1 );

  fflush( NULL );
}

void Emulator::param( Parser::Param *act )
{
  assert( act->char_present );
  assert( (act->ch == ';') || ( (act->ch >= '0') && (act->ch <= '9') ) );
  if ( params.length() < 100 ) {
    /* enough for 16 five-char params plus 15 semicolons */
    params.push_back( act->ch );
  }
}

void Emulator::collect( Parser::Collect *act )
{
  assert( act->char_present );
  if ( ( dispatch_chars.length() < 8 ) /* never should need more than 2 */
       && ( act->ch <= 255 ) ) {  /* ignore non-8-bit */
    dispatch_chars.push_back( act->ch );
  }
}

void Emulator::clear( void )
{
  params.clear();
  dispatch_chars.clear();
}

void Emulator::CSI_dispatch( Parser::CSI_Dispatch *act )
{
  /* add final char to dispatch key */
  assert( act->char_present );
  Parser::Collect act2;
  act2.char_present = true;
  act2.ch = act->ch;
  collect( &act2 ); 

  if ( dispatch_chars == "K" ) {
    CSI_EL();
  } else if ( (dispatch_chars == "A")
	      || (dispatch_chars == "B")
	      || (dispatch_chars == "C")
	      || (dispatch_chars == "D")
	      || (dispatch_chars == "H") ) {
    CSI_cursormove();
  }
}

void Emulator::parse_params( void )
{
  parsed_params.clear();
  const char *str = params.c_str();
  const char *segment_begin = str;

  while ( 1 ) {
    const char *segment_end = strchr( segment_begin, ';' );
    if ( segment_end == NULL ) {
      break;
    }

    errno = 0;
    char *endptr;
    int val = strtol( segment_begin, &endptr, 10 );
    if ( (errno == 0) && (endptr != segment_begin) ) {
      parsed_params.push_back( val );
    }

    segment_begin = segment_end + 1;
  }

  /* get last param */
  errno = 0;
  char *endptr;
  int val = strtol( segment_begin, &endptr, 10 );
  if ( (errno == 0) && (endptr != segment_begin) ) {
    parsed_params.push_back( val );
  }
}
