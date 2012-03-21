/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <typeinfo>

#include "terminal.h"
#include "swrite.h"

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

void Emulator::execute( const Parser::Execute *act )
{
  dispatch.dispatch( CONTROL, act, &fb );
}

void Emulator::print( const Parser::Print *act )
{
  assert( act->char_present );

  int chwidth = act->ch == L'\0' ? -1 : wcwidth( act->ch );

  Cell *this_cell = fb.get_mutable_cell();

  Cell *combining_cell = fb.get_combining_cell(); /* can be null if we were resized */

  switch ( chwidth ) {
  case 1: /* normal character */
  case 2: /* wide character */
    if ( fb.ds.auto_wrap_mode && fb.ds.next_print_will_wrap ) {
      fb.get_mutable_row( -1 )->wrap = true;
      fb.ds.move_col( 0 );
      fb.move_rows_autoscroll( 1 );
    }

    /* wrap 2-cell chars if no room, even without will-wrap flag */
    if ( fb.ds.auto_wrap_mode
	 && (chwidth == 2)
	 && (fb.ds.get_cursor_col() == fb.ds.get_width() - 1) ) {
      fb.reset_cell( this_cell );
      fb.get_mutable_row( -1 )->wrap = false;
      /* There doesn't seem to be a consistent way to get the
	 downstream terminal emulator to set the wrap-around
	 copy-and-paste flag on a row that ends with an empty cell
	 because a wide char was wrapped to the next line. */
      fb.ds.move_col( 0 );
      fb.move_rows_autoscroll( 1 );
    }

    if ( fb.ds.insert_mode ) {
      for ( int i = 0; i < chwidth; i++ ) {
	fb.insert_cell( fb.ds.get_cursor_row(), fb.ds.get_cursor_col() );
      }
    }

    this_cell = fb.get_mutable_cell();

    fb.reset_cell( this_cell );
    this_cell->contents.push_back( act->ch );
    this_cell->width = chwidth;
    fb.apply_renditions_to_current_cell();

    if ( chwidth == 2 ) { /* erase overlapped cell */
      if ( fb.ds.get_cursor_col() + 1 < fb.ds.get_width() ) {
	fb.reset_cell( fb.get_mutable_cell( fb.ds.get_cursor_row(), fb.ds.get_cursor_col() + 1 ) );
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
    break;
  }
}

void Emulator::CSI_dispatch( const Parser::CSI_Dispatch *act )
{
  dispatch.dispatch( CSI, act, &fb );
}

void Emulator::OSC_end( const Parser::OSC_End *act )
{
  dispatch.OSC_dispatch( act, &fb );
}

void Emulator::Esc_dispatch( const Parser::Esc_Dispatch *act )
{
  /* handle 7-bit ESC-encoding of C1 control characters */
  if ( (dispatch.get_dispatch_chars().size() == 0)
       && (0x40 <= act->ch)
       && (act->ch <= 0x5F) ) {
    Parser::Esc_Dispatch act2 = *act;
    act2.ch += 0x40;
    dispatch.dispatch( CONTROL, &act2, &fb );
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
  return std::string( "\033[?1l\033[r\033[0m" );
}

void Emulator::resize( size_t s_width, size_t s_height )
{
  fb.resize( s_width, s_height );
}

bool Emulator::operator==( Emulator const &x ) const
{
  /* dispatcher and user are irrelevant for us */
  return ( fb == x.fb );
}
