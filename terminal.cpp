#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <typeinfo>

#include "terminal.hpp"
#include "swrite.hpp"

using namespace Terminal;

Emulator::Emulator( size_t s_width, size_t s_height )
  : fb( s_width, s_height ), dispatch(), user(), display( s_width, s_height )
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

  Cell *combining_cell = fb.get_combining_cell(); /* can be null if we were resized */

  switch ( chwidth ) {
  case 1: /* normal character */
  case 2: /* wide character */
    if ( fb.ds.auto_wrap_mode && fb.ds.next_print_will_wrap ) {
      fb.get_row( -1 )->wrap = true;
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
    if ( combining_cell == NULL ) { /* character is now offscreen */
      act->handled = true;
      break;
    }

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

std::string Emulator::open( void )
{
  char appmode[ 6 ] = { 0x1b, '[', '?', '1', 'h', 0 };
  return std::string( appmode );
}

std::string Emulator::close( void )
{
  char ansimode[ 6 ] = { 0x1b, '[', '?', '1', 'l', 0 };
  return std::string( ansimode );
}

void Emulator::resize( size_t s_width, size_t s_height )
{
  fb.resize( s_width, s_height );
}
