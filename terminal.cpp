#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <typeinfo>

#include "terminal.hpp"
#include "swrite.hpp"

using namespace Terminal;

Emulator::Emulator( size_t s_width, size_t s_height )
  : parser(), fb( s_width, s_height ), as(), terminal_to_host()
{}

std::string Emulator::input( char c, int actfd )
{
  terminal_to_host.clear();

  std::vector<Parser::Action *> vec = parser.input( c );

  for ( std::vector<Parser::Action *>::iterator i = vec.begin();
	i != vec.end();
	i++ ) {
    Parser::Action *act = *i;

    act->act_on_terminal( this );

    /* print out debugging information */
    if ( (actfd > 0) && ( !act->handled ) ) {
      char actsum[ 64 ];
      snprintf( actsum, 64, "%s%s ", act->str().c_str(), as.str().c_str() );
      swrite( actfd, actsum );
    }
    delete act;
  }

  /* the user is supposed to send this string to the host, as if typed */
  return terminal_to_host;
}

void Emulator::execute( Parser::Execute *act )
{
  assert( act->char_present );

  switch ( act->ch ) {
  case 0x0a: /* LF */
    fb.cursor_row++;
    fb.autoscroll();
    fb.newgrapheme();
    act->handled = true;
    break;

  case 0x0d: /* CR */
    fb.cursor_col = 0;
    fb.newgrapheme();
    act->handled = true;
    break;
    
  case 0x08: /* BS */
    if ( fb.cursor_col > 0 ) {
      fb.cursor_col--;
      fb.newgrapheme(); /* this is not xterm's behavior */
      act->handled = true;
    }
    break;
  }
}

void Emulator::print( Parser::Print *act )
{
  assert( act->char_present );

  if ( (fb.width == 0) || (fb.height == 0) ) {
    return;
  }

  assert( fb.cursor_row < fb.height ); /* must be on screen */
  assert( fb.cursor_col <= fb.width + 1 ); /* two off is ok */
  assert( fb.combining_char_row < fb.height );
  assert( fb.combining_char_col < fb.width );

  int chwidth = act->ch == L'\0' ? -1 : wcwidth( act->ch );

  Cell *this_cell;

  switch ( chwidth ) {
  case 1: /* normal character */
  case 2: /* wide character */
    if ( fb.cursor_col >= fb.width ) { /* wrap */
      fb.cursor_col = 0;
      fb.cursor_row++;
    }

    fb.autoscroll();
    fb.newgrapheme();

    this_cell = &fb.rows[ fb.cursor_row ].cells[ fb.cursor_col ];
    this_cell->reset();
    this_cell->contents.push_back( act->ch );

    if ( (fb.cursor_col < fb.width - 1) && (chwidth == 2) ) {
      Cell *next_cell = &fb.rows[ fb.cursor_row ].cells[ fb.cursor_col + 1 ];
      this_cell->overlapped_cells.push_back( next_cell );
      next_cell->overlapping_cell = this_cell;
    }

    fb.cursor_col += chwidth;
    act->handled = true;
    break;
  case 0: /* combining character */
    if ( fb.rows[ fb.combining_char_row ].cells[ fb.combining_char_col ].contents.size() == 0 ) {
      /* cell starts with combining character */
      fb.rows[ fb.combining_char_row ].cells[ fb.combining_char_col ].fallback = true;
      assert( fb.cursor_col == fb.combining_char_col );
      assert( fb.cursor_row == fb.combining_char_row );
      assert( fb.cursor_col < fb.width );
      fb.cursor_col++;
      /* a combining character should never be able to wrap us */
    }

    if ( fb.rows[ fb.combining_char_row ].cells[ fb.combining_char_col ].contents.size() < 16 ) { /* seems like a reasonable limit on combining character */
      fb.rows[ fb.combining_char_row ].cells[ fb.combining_char_col ].contents.push_back( act->ch );
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

  for ( int y = 0; y < fb.height; y++ ) {
    for ( int x = 0; x < fb.width; x++ ) {
      char curmove[ 32 ];
      snprintf( curmove, 32, "\033[%d;%dH", y + 1, x + 1 );
      screen.append( curmove );
      Cell *cell = &fb.rows[ y ].cells[ x ];
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
  snprintf( curmove, 32, "\033[%d;%dH", fb.cursor_row + 1, fb.cursor_col + 1 );
  screen.append( curmove );

  swrite( fd, screen.c_str() );
}

void Emulator::CSI_dispatch( Parser::CSI_Dispatch *act )
{
  /* add final char to dispatch key */
  assert( act->char_present );
  Parser::Collect act2;
  act2.char_present = true;
  act2.ch = act->ch;
  as.collect( &act2 ); 

  std::string dispatch_chars = as.dispatch_chars;

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
  as.collect( &act2 ); 
  
  if ( as.dispatch_chars == "#8" ) {
    Esc_DECALN();
    act->handled = true;
  }
}
