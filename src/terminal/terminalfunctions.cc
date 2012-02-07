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

#include <unistd.h>
#include <string>

#include "terminaldispatcher.h"
#include "terminalframebuffer.h"
#include "parseraction.h"

using namespace Terminal;

/* Terminal functions -- routines activated by CSI, escape or a control char */

static void clearline( Framebuffer *fb, int row, int start, int end )
{
  for ( int col = start; col <= end; col++ ) {
    fb->reset_cell( fb->get_mutable_cell( row, col ) );
  }
}

/* erase in line */
void CSI_EL( Framebuffer *fb, Dispatcher *dispatch )
{
  switch ( dispatch->getparam( 0, 0 ) ) {
  case 0: /* default: active position to end of line, inclusive */
    clearline( fb, -1, fb->ds.get_cursor_col(), fb->ds.get_width() - 1 );    
    break;
  case 1: /* start of screen to active position, inclusive */
    clearline( fb, -1, 0, fb->ds.get_cursor_col() );
    break;
  case 2: /* all of line */
    fb->reset_row( fb->get_mutable_row( -1 ) );
    break;
  }
}

static Function func_CSI_EL( CSI, "K", CSI_EL );

/* erase in display */
void CSI_ED( Framebuffer *fb, Dispatcher *dispatch ) {
  switch ( dispatch->getparam( 0, 0 ) ) {
  case 0: /* active position to end of screen, inclusive */
    clearline( fb, -1, fb->ds.get_cursor_col(), fb->ds.get_width() - 1 );
    for ( int y = fb->ds.get_cursor_row() + 1; y < fb->ds.get_height(); y++ ) {
      fb->reset_row( fb->get_mutable_row( y ) );
    }
    break;
  case 1: /* start of screen to active position, inclusive */
    for ( int y = 0; y < fb->ds.get_cursor_row(); y++ ) {
      fb->reset_row( fb->get_mutable_row( y ) );
    }
    clearline( fb, -1, 0, fb->ds.get_cursor_col() );
    break;
  case 2: /* entire screen */
    for ( int y = 0; y < fb->ds.get_height(); y++ ) {
      fb->reset_row( fb->get_mutable_row( y ) );
    }
    break;
  }
}

static Function func_CSI_ED( CSI, "J", CSI_ED );

/* cursor movement -- relative and absolute */
void CSI_cursormove( Framebuffer *fb, Dispatcher *dispatch )
{
  int num = dispatch->getparam( 0, 1 );

  switch ( dispatch->get_dispatch_chars()[ 0 ] ) {
  case 'A':
    fb->ds.move_row( -num, true );
    break;
  case 'B':
    fb->ds.move_row( num, true );
    break;
  case 'C':
    fb->ds.move_col( num, true );
    break;
  case 'D':
    fb->ds.move_col( -num, true );
    break;
  case 'H':
  case 'f':
    int x = dispatch->getparam( 0, 1 );
    int y = dispatch->getparam( 1, 1 );
    fb->ds.move_row( x - 1 );
    fb->ds.move_col( y - 1 );
  }
}

static Function func_CSI_cursormove_A( CSI, "A", CSI_cursormove );
static Function func_CSI_cursormove_B( CSI, "B", CSI_cursormove );
static Function func_CSI_cursormove_C( CSI, "C", CSI_cursormove );
static Function func_CSI_cursormove_D( CSI, "D", CSI_cursormove );
static Function func_CSI_cursormove_H( CSI, "H", CSI_cursormove );
static Function func_CSI_cursormove_f( CSI, "f", CSI_cursormove );

/* device attributes */
void CSI_DA( Framebuffer *fb __attribute((unused)), Dispatcher *dispatch )
{
  dispatch->terminal_to_host.append( "\033[?62c" ); /* plain vt220 */
}

static Function func_CSI_DA( CSI, "c", CSI_DA );

/* secondary device attributes */
void CSI_SDA( Framebuffer *fb __attribute((unused)), Dispatcher *dispatch )
{
  dispatch->terminal_to_host.append( "\033[>1;10;0c" ); /* plain vt220 */
}

static Function func_CSI_SDA( CSI, ">c", CSI_SDA );

/* screen alignment diagnostic */
void Esc_DECALN( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  for ( int y = 0; y < fb->ds.get_height(); y++ ) {
    for ( int x = 0; x < fb->ds.get_width(); x++ ) {
      fb->reset_cell( fb->get_mutable_cell( y, x ) );
      fb->get_mutable_cell( y, x )->contents.push_back( L'E' );
    }
  }
}

static Function func_Esc_DECALN( ESCAPE, "#8", Esc_DECALN );

/* line feed */
void Ctrl_LF( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->move_rows_autoscroll( 1 );
}

/* same procedure for index, vertical tab, and form feed control codes */
static Function func_Ctrl_LF( CONTROL, "\x0a", Ctrl_LF );
static Function func_Ctrl_IND( CONTROL, "\x84", Ctrl_LF );
static Function func_Ctrl_VT( CONTROL, "\x0b", Ctrl_LF );
static Function func_Ctrl_FF( CONTROL, "\x0c", Ctrl_LF );

/* carriage return */
void Ctrl_CR( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->ds.move_col( 0 );
}

static Function func_Ctrl_CR( CONTROL, "\x0d", Ctrl_CR );

/* backspace */
void Ctrl_BS( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->ds.move_col( -1, true );
}

static Function func_Ctrl_BS( CONTROL, "\x08", Ctrl_BS );

/* reverse index -- like a backwards line feed */
void Ctrl_RI( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->move_rows_autoscroll( -1 );
}

static Function func_Ctrl_RI( CONTROL, "\x8D", Ctrl_RI );

/* newline */
void Ctrl_NEL( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->ds.move_col( 0 );
  fb->move_rows_autoscroll( 1 );
}

static Function func_Ctrl_NEL( CONTROL, "\x85", Ctrl_NEL );

/* horizontal tab */
void Ctrl_HT( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  int col = fb->ds.get_next_tab();
  if ( col == -1 ) { /* no tabs, go to end of line */
    fb->ds.move_col( fb->ds.get_width() - 1 );
  } else {
    fb->ds.move_col( col );
  }
}

static Function func_Ctrl_HT( CONTROL, "\x09", Ctrl_HT );

/* horizontal tab set */
void Ctrl_HTS( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->ds.set_tab();
}

static Function func_Ctrl_HTS( CONTROL, "\x88", Ctrl_HTS );

/* tabulation clear */
void CSI_TBC( Framebuffer *fb, Dispatcher *dispatch )
{
  int param = dispatch->getparam( 0, 0 );
  switch ( param ) {
  case 0: /* clear this tab stop */
    fb->ds.clear_tab( fb->ds.get_cursor_col() );    
    break;
  case 3: /* clear all tab stops */
    for ( int x = 0; x < fb->ds.get_width(); x++ ) {
      fb->ds.clear_tab( x );
    }
    break;
  }
}

static Function func_CSI_TBC( CSI, "g", CSI_TBC );

static bool *get_DEC_mode( int param, Framebuffer *fb ) {
  switch ( param ) {
  case 1: /* cursor key mode */
    return &(fb->ds.application_mode_cursor_keys);
  case 3: /* 80/132. Ignore but clear screen. */
    /* clear screen */
    fb->ds.move_row( 0 );
    fb->ds.move_col( 0 );
    for ( int y = 0; y < fb->ds.get_height(); y++ ) {
      fb->reset_row( fb->get_mutable_row( y ) );
    }
    return NULL;
  case 5: /* reverse video */
    return &(fb->ds.reverse_video);
  case 6: /* origin */
    fb->ds.move_row( 0 );
    fb->ds.move_col( 0 );
    return &(fb->ds.origin_mode);
  case 7: /* auto wrap */
    return &(fb->ds.auto_wrap_mode);
  case 25:
    return &(fb->ds.cursor_visible);
  }
  return NULL;
}

/* set private mode */
void CSI_DECSM( Framebuffer *fb, Dispatcher *dispatch )
{
  for ( int i = 0; i < dispatch->param_count(); i++ ) {
    bool *mode = get_DEC_mode( dispatch->getparam( i, 0 ), fb );
    if ( mode ) {
      *mode = true;
    }
  }
}

/* clear private mode */
void CSI_DECRM( Framebuffer *fb, Dispatcher *dispatch )
{
  for ( int i = 0; i < dispatch->param_count(); i++ ) {
    bool *mode = get_DEC_mode( dispatch->getparam( i, 0 ), fb );
    if ( mode ) {
      *mode = false;
    }
  }
}

static Function func_CSI_DECSM( CSI, "?h", CSI_DECSM );
static Function func_CSI_DECRM( CSI, "?l", CSI_DECRM );

static bool *get_ANSI_mode( int param, Framebuffer *fb ) {
  switch ( param ) {
  case 4: /* insert/replace mode */
    return &(fb->ds.insert_mode);
  }
  return NULL;
}

/* set mode */
void CSI_SM( Framebuffer *fb, Dispatcher *dispatch )
{
  for ( int i = 0; i < dispatch->param_count(); i++ ) {
    bool *mode = get_ANSI_mode( dispatch->getparam( i, 0 ), fb );
    if ( mode ) {
      *mode = true;
    }
  }
}

/* clear mode */
void CSI_RM( Framebuffer *fb, Dispatcher *dispatch )
{
  for ( int i = 0; i < dispatch->param_count(); i++ ) {
    bool *mode = get_ANSI_mode( dispatch->getparam( i, 0 ), fb );
    if ( mode ) {
      *mode = false;
    }
  }
}

static Function func_CSI_SM( CSI, "h", CSI_SM );
static Function func_CSI_RM( CSI, "l", CSI_RM );

/* set top and bottom margins */
void CSI_DECSTBM( Framebuffer *fb, Dispatcher *dispatch )
{
  int top = dispatch->getparam( 0, 1 );
  int bottom = dispatch->getparam( 1, fb->ds.get_height() );

  fb->ds.set_scrolling_region( top - 1, bottom - 1 );
  fb->ds.move_row( 0 );
  fb->ds.move_col( 0 );
}

static Function func_CSI_DECSTMB( CSI, "r", CSI_DECSTBM );

/* terminal bell */
void Ctrl_BEL( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) ) {
  fb->ring_bell();
}

static Function func_Ctrl_BEL( CONTROL, "\x07", Ctrl_BEL );

/* select graphics rendition -- e.g., bold, blinking, etc. */
void CSI_SGR( Framebuffer *fb, Dispatcher *dispatch )
{
  for ( int i = 0; i < dispatch->param_count(); i++ ) {
    int rendition = dispatch->getparam( i, 0 );
    fb->ds.add_rendition( rendition );
  }
}

static Function func_CSI_SGR( CSI, "m", CSI_SGR, false ); /* changing renditions doesn't clear wrap flag */

/* save and restore cursor */
void Esc_DECSC( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->ds.save_cursor();
}

void Esc_DECRC( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->ds.restore_cursor();
}

static Function func_Esc_DECSC( ESCAPE, "7", Esc_DECSC );
static Function func_Esc_DECRC( ESCAPE, "8", Esc_DECRC );

/* device status report -- e.g., cursor position (used by resize) */
void CSI_DSR( Framebuffer *fb, Dispatcher *dispatch )
{
  int param = dispatch->getparam( 0, 0 );

  switch ( param ) {
  case 5: /* device status report requested */
    dispatch->terminal_to_host.append( "\033[0n" );
    break;
  case 6: /* report of active position requested */
    char cpr[ 32 ];
    snprintf( cpr, 32, "\033[%d;%dR",
	      fb->ds.get_cursor_row() + 1,
	      fb->ds.get_cursor_col() + 1 );
    dispatch->terminal_to_host.append( cpr );
    break;
  }
}

static Function func_CSI_DSR( CSI, "n", CSI_DSR );

/* insert line */
void CSI_IL( Framebuffer *fb, Dispatcher *dispatch )
{
  int lines = dispatch->getparam( 0, 1 );

  for ( int i = 0; i < lines; i++ ) {
    fb->insert_line( fb->ds.get_cursor_row() );
  }

  /* vt220 manual and Ecma-48 say to move to first column */
  /* but xterm and gnome-terminal don't */
  fb->ds.move_col( 0 );
}

static Function func_CSI_IL( CSI, "L", CSI_IL );

/* delete line */
void CSI_DL( Framebuffer *fb, Dispatcher *dispatch )
{
  int lines = dispatch->getparam( 0, 1 );

  for ( int i = 0; i < lines; i++ ) {
    fb->delete_line( fb->ds.get_cursor_row() );
  }

  /* same story -- xterm and gnome-terminal don't
     move to first column */
  fb->ds.move_col( 0 );
}

static Function func_CSI_DL( CSI, "M", CSI_DL );

/* insert characters */
void CSI_ICH( Framebuffer *fb, Dispatcher *dispatch )
{
  int cells = dispatch->getparam( 0, 1 );

  for ( int i = 0; i < cells; i++ ) {
    fb->insert_cell( fb->ds.get_cursor_row(), fb->ds.get_cursor_col() );
  }
}

static Function func_CSI_ICH( CSI, "@", CSI_ICH );

/* delete character */
void CSI_DCH( Framebuffer *fb, Dispatcher *dispatch )
{
  int cells = dispatch->getparam( 0, 1 );

  for ( int i = 0; i < cells; i++ ) {
    fb->delete_cell( fb->ds.get_cursor_row(), fb->ds.get_cursor_col() );
  }
}

static Function func_CSI_DCH( CSI, "P", CSI_DCH );

/* line position absolute */
void CSI_VPA( Framebuffer *fb, Dispatcher *dispatch )
{
  int row = dispatch->getparam( 0, 1 );
  fb->ds.move_row( row - 1 );
}

static Function func_CSI_VPA( CSI, "d", CSI_VPA );

/* character position absolute */
void CSI_HPA( Framebuffer *fb, Dispatcher *dispatch )
{
  int col = dispatch->getparam( 0, 1 );
  fb->ds.move_col( col - 1 );
}

static Function func_CSI_CHA( CSI, "G", CSI_HPA ); /* ECMA-48 name: CHA */
static Function func_CSI_HPA( CSI, "\x60", CSI_HPA ); /* ECMA-48 name: HPA */

/* erase character */
void CSI_ECH( Framebuffer *fb, Dispatcher *dispatch )
{
  int num = dispatch->getparam( 0, 1 );
  int limit = fb->ds.get_cursor_col() + num - 1;
  if ( limit >= fb->ds.get_width() ) {
    limit = fb->ds.get_width() - 1;
  }

  clearline( fb, -1, fb->ds.get_cursor_col(), limit );
}

static Function func_CSI_ECH( CSI, "X", CSI_ECH );

/* reset to initial state */
void Esc_RIS( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->reset();
}

static Function func_Esc_RIS( ESCAPE, "c", Esc_RIS );

/* soft reset */
void CSI_DECSTR( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  fb->soft_reset();
}

static Function func_CSI_DECSTR( CSI, "!p", CSI_DECSTR );

/* xterm uses an Operating System Command to set the window title */
void Dispatcher::OSC_dispatch( const Parser::OSC_End *act, Framebuffer *fb )
{
  if ( OSC_string.size() >= 2 ) {
    if ( (OSC_string[ 0 ] == L'0')
	 && (OSC_string[ 1 ] == L';') ) {
      std::deque<wchar_t> newtitle( OSC_string.begin(), OSC_string.end() );
      newtitle.erase( newtitle.begin() );
      newtitle.erase( newtitle.begin() );
      fb->set_window_title( newtitle );
      act->handled = true;
    }
  }
}

/* scroll down or terminfo indn */
void CSI_SD( Framebuffer *fb, Dispatcher *dispatch )
{
  fb->scroll( dispatch->getparam( 0, 1 ) );
}

static Function func_CSI_SD( CSI, "S", CSI_SD );

/* scroll up or terminfo rin */
void CSI_SU( Framebuffer *fb, Dispatcher *dispatch )
{
  fb->scroll( -dispatch->getparam( 0, 1 ) );
}

static Function func_CSI_SU( CSI, "T", CSI_SU );
