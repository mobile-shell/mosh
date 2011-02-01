#include <unistd.h>

#include "terminaldispatcher.hpp"
#include "terminalframebuffer.hpp"

using namespace Terminal;

/* Terminal functions -- routines activated by CSI, escape or a C1 or C2 control char */

static void clearline( Framebuffer *fb, int row, int start, int end )
{
  for ( int col = start; col <= end; col++ ) {
    fb->get_cell( row, col )->reset();
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
    clearline( fb, -1, 0, fb->ds.get_width() - 1 );
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
      clearline( fb, y, 0, fb->ds.get_width() - 1 );
    }
    break;
  case 1: /* start of screen to active position, inclusive */
    for ( int y = 0; y < fb->ds.get_cursor_row(); y++ ) {
      clearline( fb, y, 0, fb->ds.get_width() - 1 );
    }
    clearline( fb, -1, 0, fb->ds.get_cursor_col() );
    break;
  case 2: /* entire screen */
    for ( int y = 0; y < fb->ds.get_height(); y++ ) {
      clearline( fb, y, 0, fb->ds.get_width() - 1 );
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
  dispatch->terminal_to_host.append( "\033[?1;0c" );
}

static Function func_CSI_DA( CSI, "c", CSI_DA );

/* screen alignment diagnostic */
void Esc_DECALN( Framebuffer *fb, Dispatcher *dispatch __attribute((unused)) )
{
  for ( int y = 0; y < fb->ds.get_height(); y++ ) {
    for ( int x = 0; x < fb->ds.get_width(); x++ ) {
      fb->get_cell( y, x )->reset();
      fb->get_cell( y, x )->contents.push_back( L'E' );
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
  case 3: /* 80/132 */
    /* clear screen */
    fb->ds.move_row( 0 );
    fb->ds.move_col( 0 );
    for ( int y = 0; y < fb->ds.get_height(); y++ ) {
      clearline( fb, y, 0, fb->ds.get_width() - 1 );
    }
    return NULL;
  case 6: /* origin */
    fb->ds.move_row( 0 );
    fb->ds.move_col( 0 );
    return &(fb->ds.origin_mode);
  case 7: /* auto wrap */
    return &(fb->ds.auto_wrap_mode);
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

/* terminal bell -- ignored for now */
void Ctrl_BEL( Framebuffer *fb __attribute((unused)), Dispatcher *dispatch __attribute((unused)) )
{}

static Function func_Ctrl_BEL( CONTROL, "\x07", Ctrl_BEL );

/* select graphics rendition -- e.g., bold, blinking, etc. */
void CSI_SGR( Framebuffer *fb, Dispatcher *dispatch )
{
  for ( int i = 0; i < dispatch->param_count(); i++ ) {
    int rendition = dispatch->getparam( i, 0 );
    if ( rendition == 0 ) {
      fb->ds.clear_renditions();
    } else {
      fb->ds.add_rendition( rendition );
    }
  }
}

static Function func_CSI_SGR( CSI, "m", CSI_SGR );
