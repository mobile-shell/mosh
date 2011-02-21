#include <assert.h>

#include "terminaldisplay.hpp"

using namespace Terminal;

/* Print a new "frame" to the terminal, using ANSI/ECMA-48 escape codes. */

std::string Display::new_frame( Framebuffer &f )
{
  FrameState frame;

  char tmp[ 64 ];

  /* has window title changed? */
  if ( (!initialized)
       || (f.get_window_title() != last_frame.get_window_title()) ) {
      /* set window title */
    frame.append( "\033]0;[rtm] " );
    std::vector<wchar_t> window_title = f.get_window_title();
    for ( std::vector<wchar_t>::iterator i = window_title.begin();
	  i != window_title.end();
	  i++ ) {
      snprintf( tmp, 64, "%lc", *i );
      frame.append( tmp );
    }
    frame.append( "\033\\" );
  }

  /* has reverse video state changed? */
  if ( (!initialized)
       || (f.ds.reverse_video != last_frame.ds.reverse_video) ) {
    /* set reverse video */
    snprintf( tmp, 64, "\033[?5%c", (f.ds.reverse_video ? 'h' : 'l') );
    frame.append( tmp );
  }

  /* has size changed? */
  if ( (!initialized)
       || (f.ds.get_width() != last_frame.ds.get_width())
       || (f.ds.get_height() != last_frame.ds.get_height()) ) {
    /* clear screen */
    frame.append( "\033[0m\033[H\033[2J" );
    initialized = false;
    cursor_x = cursor_y = 0;
    current_rendition_string = "\033[0m";
  }

  /* shortcut -- has display moved up by a certain number of lines? */
  frame.y = 0;

  if ( initialized ) {
    int lines_scrolled = 0;
    int scroll_height = 0;

    for ( int row = 0; row < f.ds.get_height(); row++ ) {
      if ( *(f.get_row( 0 )) == *(last_frame.get_row( row )) ) {
	/* found a scroll */
	lines_scrolled = row;
	scroll_height = 1;

	/* how big is the region that was scrolled? */
	for ( int region_height = 1;
	      lines_scrolled + region_height < f.ds.get_height();
	      region_height++ ) {
	  if ( *(f.get_row( region_height ))
	       == *(last_frame.get_row( lines_scrolled + region_height )) ) {
	    scroll_height = region_height + 1;
	  } else {
	    break;
	  }
	}

	break;
      }
    }

    if ( scroll_height ) {
      frame.y = scroll_height;

      if ( lines_scrolled ) {
	if ( cursor_y != f.ds.get_height() - 1 ) {
	  snprintf( tmp, 64, "\033[%d;%dH", f.ds.get_height(), 1 );
	  frame.append( tmp );

	  cursor_y = f.ds.get_height() - 1;
	  cursor_x = 0;
	}

	if ( current_rendition_string != "\033[0m" ) {
	  frame.append( "\033[0m" );
	  current_rendition_string = "\033[0m";
	}

	for ( int i = 0; i < lines_scrolled; i++ ) {
	  frame.append( "\n" );
	}

	for ( int i = 0; i < f.ds.get_height(); i++ ) {
	  if ( i + lines_scrolled < f.ds.get_height() ) {
	    *(last_frame.get_row( i )) = *(last_frame.get_row( i + lines_scrolled ));
	  } else {
	    last_frame.get_row( i )->reset( 0 );
	  }
	}
      }
    }
  }

  /* iterate for every cell */
  for ( ; frame.y < f.ds.get_height(); frame.y++ ) {
    int last_x = 0;
    for ( frame.x = 0;
	  frame.x < f.ds.get_width(); /* let put_cell() handle advance */ ) {
      last_x = frame.x;
      put_cell( frame, f );

      /* To hint that a word-select should group the end of one line
	 with the beginning of the next, we let the real cursor
	 actually wrap around in cases where it wrapped around for us. */

      if ( (cursor_x >= f.ds.get_width())
	   && (frame.y < f.ds.get_height() - 1)
	   && f.get_row( frame.y )->wrap
	   && (!initialized || !last_frame.get_row( frame.y )->wrap) ) {
	/* next write will wrap */
	cursor_x = 0;
	cursor_y++;
      }
    }

    /* Turn off wrap */
    if ( (frame.y < f.ds.get_height() - 1)
	 && (!f.get_row( frame.y )->wrap)
	 && (!initialized || last_frame.get_row( frame.y )->wrap) ) {
      frame.x = last_x;
      if ( initialized ) {
	last_frame.reset_cell( last_frame.get_cell( frame.y, frame.x ) );
      }

      snprintf( tmp, 64, "\033[%d;%dH\033[K", frame.y + 1, frame.x + 1 );
      frame.append( tmp );
      cursor_x = frame.x;

      put_cell( frame, f );
    }
  }

  /* has cursor location changed? */
  if ( (!initialized)
       || (f.ds.get_cursor_row() != cursor_y)
       || (f.ds.get_cursor_col() != cursor_x) ) {
    snprintf( tmp, 64, "\033[%d;%dH", f.ds.get_cursor_row() + 1,
	      f.ds.get_cursor_col() + 1 );
    frame.append( tmp );
    cursor_x = f.ds.get_cursor_col();
    cursor_y = f.ds.get_cursor_row();
  }

  /* has cursor visibility changed? */
  if ( (!initialized)
       || (f.ds.cursor_visible != last_frame.ds.cursor_visible) ) {
    if ( f.ds.cursor_visible ) {
      frame.append( "\033[?25h" );
    } else {
      frame.append( "\033[?25l" );
    }
  }

  last_frame = f;
  initialized = true;

  return frame.str;
}

void Display::put_cell( FrameState &frame, Framebuffer &f )
{
  char tmp[ 64 ];

  Cell *cell = f.get_cell( frame.y, frame.x );

  if ( initialized
       && ( *cell == *(last_frame.get_cell( frame.y, frame.x )) ) ) {
    frame.x += cell->width;
    return;
  }

  if ( (frame.x != cursor_x) || (frame.y != cursor_y) ) {
    snprintf( tmp, 64, "\033[%d;%dH", frame.y + 1, frame.x + 1 );
    frame.append( tmp );
    cursor_x = frame.x;
    cursor_y = frame.y;
  }

  std::string rendition_str = cell->renditions.sgr();

  if ( current_rendition_string != rendition_str ) {
    /* print renditions */
    frame.append( rendition_str );
    current_rendition_string = rendition_str;
  }

  if ( cell->contents.empty() ) {
    /* see how far we can stretch a clear */
    int clear_count = 0;
    for ( int col = frame.x; col < f.ds.get_width(); col++ ) {
      Cell *other_cell = f.get_cell( frame.y, col );
      if ( (cell->renditions == other_cell->renditions)
	   && (other_cell->contents.empty()) ) {
	clear_count++;
      } else {
	break;
      }
    }
    snprintf( tmp, 64, "\033[%dX", clear_count );
    frame.append( tmp );

    frame.x += clear_count;
    return;
  }

  /* cells that begin with combining character get combiner attached to no-break space */
  if ( cell->fallback ) {
    snprintf( tmp, 64, "%lc", 0xA0 );
    frame.append( tmp );
  }

  for ( std::vector<wchar_t>::iterator i = cell->contents.begin();
	i != cell->contents.end();
	i++ ) {
    snprintf( tmp, 64, "%lc", *i );
    frame.append( tmp );
  }

  frame.x += cell->width;
  cursor_x += cell->width;
}
