#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <typeinfo>

#include "terminal.hpp"
#include "swrite.hpp"

using namespace Terminal;

Cell::Cell()
  : overlapping_cell( NULL ),
    contents(),
    overlapped_cells(),
    fallback( false )
{}

Cell::Cell( const Cell &x )
  : overlapping_cell( x.overlapping_cell ),
    contents( x.contents ),
    overlapped_cells( x.overlapped_cells ),
    fallback( x.fallback )
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

Emulator::Emulator( size_t s_width, size_t s_height )
  : parser(),
    width( s_width ), height( s_height ),
    cursor_col( 0 ), cursor_row( 0 ),
    combining_char_col( 0 ), combining_char_row( 0 ),
    rows( height, Row( width ) ),
    params(), dispatch_chars(), terminal_to_host(),
    errors(), parsed_params()
{

}

Emulator::~Emulator()
{

}

std::string Emulator::input( char c, int actfd )
{
  terminal_to_host.clear();

  std::vector<Parser::Action *> vec = parser.input( c );

  for ( std::vector<Parser::Action *>::iterator i = vec.begin();
	i != vec.end();
	i++ ) {
    Parser::Action *act = *i;

    act->act_on_terminal( this );

    if ( (actfd > 0) && ( !act->handled ) ) {
      char actsum[ 64 ];
      char thechar[ 10 ] = { 0 };
      if ( act->char_present ) {
	if ( isprint( act->ch ) ) {
	  snprintf( thechar, 10, "(%lc)", act->ch );
	} else {
	  snprintf( thechar, 10, "(0x%x)", act->ch );
	}
      }
      snprintf( actsum, 64, "%s%s[disp=%s,param=%s] ",
		act->name().c_str(),
		thechar,
		dispatch_chars.c_str(),
		params.c_str() );

      swrite( actfd, actsum );
    }

    delete act;
  }

  return terminal_to_host;
}

void Emulator::scroll( int N )
{
  assert( N >= 0 );

  for ( int i = 0; i < N; i++ ) {
    rows.pop_front();
    rows.push_back( Row( width ) );
    cursor_row--;
    combining_char_row--;
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
    newgrapheme();
    act->handled = true;
    break;

  case 0x0d: /* CR */
    cursor_col = 0;
    newgrapheme();
    act->handled = true;
    break;
    
  case 0x08: /* BS */
    if ( cursor_col > 0 ) {
      cursor_col--;
      newgrapheme(); /* this is not xterm's behavior */
      act->handled = true;
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
  assert( cursor_col <= width + 1 ); /* two off is ok */
  assert( combining_char_row < height );
  assert( combining_char_col < width );

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

    this_cell = &rows[ cursor_row ].cells[ cursor_col ];
    this_cell->reset();
    this_cell->contents.push_back( act->ch );

    newgrapheme();

    if ( (cursor_col < width - 1) && (chwidth == 2) ) {
      Cell *next_cell = &rows[ cursor_row ].cells[ cursor_col + 1 ];
      this_cell->overlapped_cells.push_back( next_cell );
      next_cell->overlapping_cell = this_cell;
    }

    cursor_col += chwidth;
    act->handled = true;
    break;
  case 0: /* combining character */
    if ( rows[ combining_char_row ].cells[ combining_char_col ].contents.size() == 0 ) {
      /* cell starts with combining character */
      rows[ combining_char_row ].cells[ combining_char_col ].fallback = true;
      assert( cursor_col == combining_char_col );
      assert( cursor_row == combining_char_row );
      assert( cursor_col < width );
      cursor_col++;
      /* a combining character should never be able to wrap us */
    }

    if ( rows[ combining_char_row ].cells[ combining_char_col ].contents.size() < 16 ) { /* seems like a reasonable limit on combining character */
      rows[ combining_char_row ].cells[ combining_char_col ].contents.push_back( act->ch );
    }
    act->handled = true;
    break;
  case -1: /* unprintable character */
    break;
  default:
    assert( false );
  }
}

void Emulator::debug_printout( int fd )
{
  std::string screen;
  screen.append( "\033[H\033[2J" );

  for ( int y = 0; y < height; y++ ) {
    for ( int x = 0; x < width; x++ ) {
      char curmove[ 32 ];
      snprintf( curmove, 32, "\033[%d;%dH", y + 1, x + 1 );
      screen.append( curmove );
      Cell *cell = &rows[ y ].cells[ x ];
      if ( cell->overlapping_cell ) continue;

      if ( cell->fallback ) {
	char utf8[ 8 ];
	snprintf( utf8, 8, "%lc", 0xA0 );
	screen.append( utf8 );
      }

      for ( std::vector<wchar_t>::iterator i = cell->contents.begin();
	    i != cell->contents.end();
	    i++ ) {
	char utf8[ 8 ];
	snprintf( utf8, 8, "%lc", *i );
	screen.append( utf8 );
      }
    }
  }

  char curmove[ 32 ];
  snprintf( curmove, 32, "\033[%d;%dH", cursor_row + 1, cursor_col + 1 );
  screen.append( curmove );

  swrite( fd, screen.c_str() );
}

void Emulator::param( Parser::Param *act )
{
  assert( act->char_present );
  assert( (act->ch == ';') || ( (act->ch >= '0') && (act->ch <= '9') ) );
  if ( params.length() < 100 ) {
    /* enough for 16 five-char params plus 15 semicolons */
    params.push_back( act->ch );
    act->handled = true;
  }
}

void Emulator::collect( Parser::Collect *act )
{
  assert( act->char_present );
  if ( ( dispatch_chars.length() < 8 ) /* never should need more than 2 */
       && ( act->ch <= 255 ) ) {  /* ignore non-8-bit */    
    dispatch_chars.push_back( act->ch );
    act->handled = true;
  }
}

void Emulator::clear( Parser::Clear *act )
{
  params.clear();
  dispatch_chars.clear();
  act->handled = true;
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
    act->handled = true;
  } else if ( dispatch_chars == "J" ) {
    CSI_ED();
    act->handled = true;
  } else if ( (dispatch_chars == "A")
	      || (dispatch_chars == "B")
	      || (dispatch_chars == "C")
	      || (dispatch_chars == "D")
	      || (dispatch_chars == "H") 
	      || (dispatch_chars == "f") ) {
    CSI_cursormove();
    act->handled = true;
  } else if ( dispatch_chars == "c" ) {
    CSI_DA();
    act->handled = true;
  }
}

void Emulator::Esc_dispatch( Parser::Esc_Dispatch *act )
{
  /* add final char to dispatch key */
  assert( act->char_present );
  Parser::Collect act2;
  act2.char_present = true;
  act2.ch = act->ch;
  collect( &act2 ); 
  
  if ( dispatch_chars == "#8" ) {
    Esc_DECALN();
    act->handled = true;
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
    if ( endptr == segment_begin ) {
      val = -1;
    }
    if ( errno == 0 ) {
      parsed_params.push_back( val );
    }

    segment_begin = segment_end + 1;
  }

  /* get last param */
  errno = 0;
  char *endptr;
  int val = strtol( segment_begin, &endptr, 10 );
  if ( endptr == segment_begin ) {
    val = -1;
  }
  if ( errno == 0 ) {
    parsed_params.push_back( val );
  }
}

int Emulator::getparam( size_t N, int defaultval )
{
  int ret = defaultval;
  if ( parsed_params.size() > N ) {
    ret = parsed_params[ N ];
  }
  if ( ret < 1 ) ret = defaultval;

  return ret;
}

void Cell::reset( void )
{
  contents.clear();
  fallback = false;

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
