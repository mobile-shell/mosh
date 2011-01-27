#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <typeinfo>

#include "terminal.hpp"
#include "swrite.hpp"

using namespace Terminal;

Emulator::Emulator( size_t s_width, size_t s_height )
  : parser(), fb( s_height, s_width ), as(), terminal_to_host()
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
    newgrapheme();

    this_cell = &rows[ cursor_row ].cells[ cursor_col ];
    this_cell->reset();
    this_cell->contents.push_back( act->ch );

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
