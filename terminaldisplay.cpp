#include "terminal.hpp"

using namespace Terminal;

/* Print a new "frame" to the terminal, using ANSI/ECMA-48 escape codes. */

std::string Display::new_frame( Framebuffer &f )
{
  f.back_color_erase();

  std::string screen;

  /* has window title changed? */
  if ( (!initialized)
       || (f.get_window_title() != last_frame.get_window_title()) ) {
      /* set window title */
    screen.append( "\033]0;[rtm] " );
    std::vector<wchar_t> window_title = f.get_window_title();
    for ( std::vector<wchar_t>::iterator i = window_title.begin();
	  i != window_title.end();
	  i++ ) {
      char utf8[ 8 ];
      snprintf( utf8, 8, "%lc", *i );
      screen.append( utf8 );
    }
    screen.append( "\033\\" );
  }

  /* has reverse video state changed? */
  if ( (!initialized)
       || (f.ds.reverse_video != last_frame.ds.reverse_video) ) {
    /* set reverse video */
    char rev[ 8 ];
    snprintf( rev, 8, "\033[?5%c", (f.ds.reverse_video ? 'h' : 'l') );
    screen.append( rev );
  }

  /* has size changed? */
  if ( (!initialized)
       || (f.ds.get_width() != last_frame.ds.get_width())
       || (f.ds.get_height() != last_frame.ds.get_height()) ) {
    /* clear screen */
    screen.append( "\033[0m\033[H\033[2J" );
    initialized = false;
    cursor_x = cursor_y = 0;
    current_rendition_string = "\033[0m";
  }

  /* iterate for every cell */
  for ( int y = 0; y < f.ds.get_height(); y++ ) {
    for ( int x = 0; x < f.ds.get_width(); /* let charwidth handle advance */ ) {
      Cell *cell = f.get_cell( y, x );

      if ( initialized
	   && ( *cell == *(last_frame.get_cell( y, x )) ) ) {
	x += cell->width;
	continue;
      }

      if ( (x != cursor_x) || (y != cursor_y) ) {
	char curmove[ 32 ];
	snprintf( curmove, 32, "\033[%d;%dH", y + 1, x + 1 );
	screen.append( curmove );
      }

      cursor_x = x;
      cursor_y = y;

      std::string rendition_str = cell->renditions.sgr();

      if ( current_rendition_string != rendition_str ) {
	/* print renditions */
	screen.append( rendition_str );
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
	char clearer[ 32 ];
	snprintf( clearer, 32, "\033[%dX", clear_count );
	screen.append( clearer );

	x += clear_count;
	continue;
      }

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
      cursor_x += cell->width;
    }
  }

  /* has cursor location changed? */
  if ( (!initialized)
       || (f.ds.get_cursor_row() != cursor_y)
       || (f.ds.get_cursor_col() != cursor_x) ) {
    char curmove[ 32 ];
    snprintf( curmove, 32, "\033[%d;%dH", f.ds.get_cursor_row() + 1,
	      f.ds.get_cursor_col() + 1 );
    screen.append( curmove );
    cursor_x = f.ds.get_cursor_col();
    cursor_y = f.ds.get_cursor_row();
  }

  /* has cursor visibility changed? */
  if ( (!initialized)
       || (f.ds.cursor_visible != last_frame.ds.cursor_visible) ) {
    if ( f.ds.cursor_visible ) {
      screen.append( "\033[?25h" );
    } else {
      screen.append( "\033[?25l" );
    }
  }

  last_frame = f;
  initialized = true;

  return screen;
}
