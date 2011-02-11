#include <assert.h>

#include "terminaldisplay.hpp"

using namespace Terminal;

/* Print a new "frame" to the terminal, using ANSI/ECMA-48 escape codes. */

std::string Display::new_frame( Framebuffer &f )
{
  /* fill in background color on any cells that have been reset
     or created since last time */
  f.back_color_erase();

  str.clear();
  char tmp[ 64 ];

  /* has window title changed? */
  if ( (!initialized)
       || (f.get_window_title() != last_frame.get_window_title()) ) {
      /* set window title */
    str.append( "\033]0;[rtm] " );
    std::vector<wchar_t> window_title = f.get_window_title();
    for ( std::vector<wchar_t>::iterator i = window_title.begin();
	  i != window_title.end();
	  i++ ) {
      snprintf( tmp, 64, "%lc", *i );
      str.append( tmp );
    }
    str.append( "\033\\" );
  }

  /* has reverse video state changed? */
  if ( (!initialized)
       || (f.ds.reverse_video != last_frame.ds.reverse_video) ) {
    /* set reverse video */
    snprintf( tmp, 64, "\033[?5%c", (f.ds.reverse_video ? 'h' : 'l') );
    str.append( tmp );
  }

  /* has size changed? */
  if ( (!initialized)
       || (f.ds.get_width() != last_frame.ds.get_width())
       || (f.ds.get_height() != last_frame.ds.get_height()) ) {
    /* clear screen */
    str.append( "\033[0m\033[H\033[2J" );
    initialized = false;
    cursor_x = cursor_y = 0;
    current_rendition_string = "\033[0m";
  }

  /* iterate for every cell */
  for ( y = 0; y < f.ds.get_height(); y++ ) {
    int last_x = 0;
    for ( x = 0; x < f.ds.get_width(); /* let charwidth handle advance */ ) {
      last_x = x;
      put_cell( f );

      /* To hint that a word-select should group the end of one line
	 with the beginning of the next, we let the real cursor
	 actually wrap around in cases where it wrapped around for us. */

      if ( (cursor_x >= f.ds.get_width())
	   && (y < f.ds.get_height() - 1)
	   && f.get_row( y )->wrap
	   && (!initialized || !last_frame.get_row( y )->wrap) ) {
	/* next write will wrap */
	cursor_x = 0;
	cursor_y++;
      }
    }

    /* Turn off wrap */
    if ( (y < f.ds.get_height() - 1)
	 && (!f.get_row( y )->wrap)
	 && (!initialized || last_frame.get_row( y )->wrap) ) {
      x = last_x;
      last_frame.get_cell( y, x )->reset();

      snprintf( tmp, 64, "\033[%d;%dH\033[K", y + 1, x + 1 );
      str.append( tmp );
      cursor_x = x;

      put_cell( f );
    }
  }

  /* has cursor location changed? */
  if ( (!initialized)
       || (f.ds.get_cursor_row() != cursor_y)
       || (f.ds.get_cursor_col() != cursor_x) ) {
    snprintf( tmp, 64, "\033[%d;%dH", f.ds.get_cursor_row() + 1,
	      f.ds.get_cursor_col() + 1 );
    str.append( tmp );
    cursor_x = f.ds.get_cursor_col();
    cursor_y = f.ds.get_cursor_row();
  }

  /* has cursor visibility changed? */
  if ( (!initialized)
       || (f.ds.cursor_visible != last_frame.ds.cursor_visible) ) {
    if ( f.ds.cursor_visible ) {
      str.append( "\033[?25h" );
    } else {
      str.append( "\033[?25l" );
    }
  }

  last_frame = f;
  initialized = true;

  return str;
}

void Display::put_cell( Framebuffer &f )
{
  char tmp[ 64 ];

  Cell *cell = f.get_cell( y, x );
  Cell *last_cell = last_frame.get_cell( y, x );

  if ( initialized
       && ( *cell == *last_cell ) ) {
    x += cell->width;
    return;
  }

  if ( (x != cursor_x) || (y != cursor_y) ) {
    snprintf( tmp, 64, "\033[%d;%dH", y + 1, x + 1 );
    str.append( tmp );
    cursor_x = x;
    cursor_y = y;
  }

  std::string rendition_str = cell->renditions.sgr();

  if ( current_rendition_string != rendition_str ) {
    /* print renditions */
    str.append( rendition_str );
    current_rendition_string = rendition_str;
  }

  if ( cell->contents.empty() ) {
    /* see how far we can stretch a clear */
    int clear_count = 0;
    for ( int col = x; col < f.ds.get_width(); col++ ) {
      Cell *other_cell = f.get_cell( y, col );
      if ( (cell->renditions == other_cell->renditions)
	   && (other_cell->contents.empty()) ) {
	clear_count++;
      } else {
	break;
      }
    }
    snprintf( tmp, 64, "\033[%dX", clear_count );
    str.append( tmp );

    x += clear_count;
    return;
  }

  /* cells that begin with combining character get combiner attached to no-break space */
  if ( cell->fallback ) {
    snprintf( tmp, 64, "%lc", 0xA0 );
    str.append( tmp );
  }

  for ( std::vector<wchar_t>::iterator i = cell->contents.begin();
	i != cell->contents.end();
	i++ ) {
    snprintf( tmp, 64, "%lc", *i );
    str.append( tmp );
  }

  x += cell->width;
  cursor_x += cell->width;
}
