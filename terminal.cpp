#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <typeinfo>

#include "terminal.hpp"
#include "swrite.hpp"

using namespace Terminal;

Emulator::Emulator( size_t s_width, size_t s_height )
  : fb( s_width, s_height ), dispatch(), user()
{}

std::string Emulator::read_octets_to_host( void )
{
  std::string ret = dispatch.terminal_to_host;
  dispatch.terminal_to_host.clear();
  return ret;
}

void Emulator::execute( Parser::Execute *act )
{
  fb.ds.next_print_will_wrap = false;
  dispatch.dispatch( CONTROL, act, &fb );
}

void Emulator::print( Parser::Print *act )
{
  assert( act->char_present );

  int chwidth = act->ch == L'\0' ? -1 : wcwidth( act->ch );

  Cell *this_cell = fb.get_cell();

  Cell *combining_cell = fb.get_combining_cell();

  switch ( chwidth ) {
  case 1: /* normal character */
  case 2: /* wide character */
    if ( fb.ds.auto_wrap_mode && fb.ds.next_print_will_wrap ) {
      fb.ds.move_col( 0 );
      fb.move_rows_autoscroll( 1 );
    }

    if ( fb.ds.insert_mode ) {
      for ( int i = 0; i < chwidth; i++ ) {
	fb.insert_cell( fb.ds.get_cursor_row(), fb.ds.get_cursor_col() );
      }
    }

    this_cell = fb.get_cell();

    this_cell->reset();
    this_cell->contents.push_back( act->ch );
    this_cell->width = chwidth;
    fb.apply_renditions_to_current_cell();

    if ( chwidth == 2 ) { /* erase overlapped cell */
      if ( fb.ds.get_cursor_col() + 1 < fb.ds.get_width() ) {
	fb.get_cell( fb.ds.get_cursor_row(), fb.ds.get_cursor_col() + 1 )->reset();
      }
    }

    fb.ds.move_col( chwidth, true, true );

    act->handled = true;
    break;
  case 0: /* combining character */
    if ( combining_cell->contents.size() == 0 ) {
      /* cell starts with combining character */
      assert( this_cell == combining_cell );
      assert( combining_cell->width == 1 );
      combining_cell->fallback = true;
      fb.ds.move_col( 1, true, true );
    }

    if ( combining_cell->contents.size() < 16 ) {
      /* seems like a reasonable limit on combining characters */
      combining_cell->contents.push_back( act->ch );
    }
    act->handled = true;
    break;
  case -1: /* unprintable character */
    break;
  default:
    assert( false );
  }
}

void Emulator::CSI_dispatch( Parser::CSI_Dispatch *act )
{
  fb.ds.next_print_will_wrap = false;
  dispatch.dispatch( CSI, act, &fb );
}

void Emulator::OSC_end( Parser::OSC_End *act )
{
  fb.ds.next_print_will_wrap = false;
  dispatch.OSC_dispatch( act, &fb );
}

void Emulator::Esc_dispatch( Parser::Esc_Dispatch *act )
{
  fb.ds.next_print_will_wrap = false;
  /* handle 7-bit ESC-encoding of C1 control characters */
  if ( (dispatch.get_dispatch_chars().size() == 0)
       && (0x40 <= act->ch)
       && (act->ch <= 0x5F) ) {
    act->ch += 0x40;
    dispatch.dispatch( CONTROL, act, &fb );
    act->ch -= 0x40;
  } else {
    dispatch.dispatch( ESCAPE, act, &fb );
  }
}

void Emulator::debug_printout( int fd )
{
  std::string screen;
  screen.append( "\033[H" );

  /* set window title */
  screen.append( "\033]0;" );
  std::vector<wchar_t> window_title = fb.get_window_title();
  for ( std::vector<wchar_t>::iterator i = window_title.begin();
	i != window_title.end();
	i++ ) {
    char utf8[ 8 ];
    snprintf( utf8, 8, "%lc", *i );
    screen.append( utf8 );
  }
  screen.append( "\033\\" );

  for ( int y = 0; y < fb.ds.get_height(); y++ ) {
    for ( int x = 0; x < fb.ds.get_width(); /* let charwidth handle advance */ ) {
      char curmove[ 32 ];
      snprintf( curmove, 32, "\033[%d;%dH\033[X", y + 1, x + 1 );
      screen.append( curmove );
      Cell *cell = fb.get_cell( y, x );

      /* print renditions */
      screen.append( "\033[0" );
      char rendition[ 32 ];
      for ( std::vector<int>::iterator i = cell->renditions.begin();
	    i != cell->renditions.end();
	    i++ ) {
	snprintf( rendition, 32, ";%d", *i );
	screen.append( rendition );
      }
      screen.append( "m" );

      /* print cell contents */

      /* cells that begin with combining character get combiner attached to no-break space */
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

      x += cell->width;
    }
  }

  char curmove[ 32 ];
  if ( fb.ds.cursor_visible ) {
    snprintf( curmove, 32, "\033[?25h\033[%d;%dH", fb.ds.get_cursor_row() + 1,
	      fb.ds.get_cursor_col() + 1 );
  } else {
    snprintf( curmove, 32, "\033[?25l" );
  }
  screen.append( curmove );

  swrite( fd, screen.c_str() );
}
