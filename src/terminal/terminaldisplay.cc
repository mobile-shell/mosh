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

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#include <stdio.h>

#include "terminaldisplay.h"

using namespace Terminal;

/* Print a new "frame" to the terminal, using ANSI/ECMA-48 escape codes. */

static const Renditions & initial_rendition( void )
{
  const static Renditions blank = Renditions( 0 );
  return blank;
}

std::string Display::new_frame( bool initialized, const Framebuffer &last, const Framebuffer &f ) const
{
  FrameState frame( last );

  char tmp[ 64 ];

  /* has bell been rung? */
  if ( f.get_bell_count() != frame.last_frame.get_bell_count() ) {
    frame.append( "\x07" );
  }

  /* has icon name or window title changed? */
  if ( has_title &&
       ( (!initialized)
         || (f.get_icon_name() != frame.last_frame.get_icon_name())
         || (f.get_window_title() != frame.last_frame.get_window_title()) ) ) {
      /* set icon name and window title */
    if ( f.get_icon_name() == f.get_window_title() ) {
      /* write combined Icon Name and Window Title */
      frame.append( "\033]0;" );
      const std::deque<wchar_t> &window_title( f.get_window_title() );
      for ( std::deque<wchar_t>::const_iterator i = window_title.begin();
            i != window_title.end();
            i++ ) {
	snprintf( tmp, 64, "%lc", *i );
	frame.append( tmp );
      }
      frame.append( "\007" );
      /* ST is more correct, but BEL more widely supported */
    } else {
      /* write Icon Name */
      frame.append( "\033]1;" );
      const std::deque<wchar_t> &icon_name( f.get_icon_name() );
      for ( std::deque<wchar_t>::const_iterator i = icon_name.begin();
	    i != icon_name.end();
	    i++ ) {
	snprintf( tmp, 64, "%lc", *i );
	frame.append( tmp );
      }
      frame.append( "\007" );

      frame.append( "\033]2;" );
      const std::deque<wchar_t> &window_title( f.get_window_title() );
      for ( std::deque<wchar_t>::const_iterator i = window_title.begin();
	    i != window_title.end();
	    i++ ) {
	snprintf( tmp, 64, "%lc", *i );
	frame.append( tmp );
      }
      frame.append( "\007" );
    }

  }

  /* has reverse video state changed? */
  if ( (!initialized)
       || (f.ds.reverse_video != frame.last_frame.ds.reverse_video) ) {
    /* set reverse video */
    snprintf( tmp, 64, "\033[?5%c", (f.ds.reverse_video ? 'h' : 'l') );
    frame.append( tmp );
  }

  /* has size changed? */
  if ( (!initialized)
       || (f.ds.get_width() != frame.last_frame.ds.get_width())
       || (f.ds.get_height() != frame.last_frame.ds.get_height()) ) {
    /* reset scrolling region */
    snprintf( tmp, 64, "\033[%d;%dr",
	      1, f.ds.get_height() );
    frame.append( tmp );

    /* clear screen */
    frame.append( "\033[0m\033[H\033[2J" );
    initialized = false;
    frame.cursor_x = frame.cursor_y = 0;
    frame.current_rendition = initial_rendition();
  } else {
    frame.cursor_x = frame.last_frame.ds.get_cursor_col();
    frame.cursor_y = frame.last_frame.ds.get_cursor_row();
    frame.current_rendition = frame.last_frame.ds.get_renditions();
  }

  /* shortcut -- has display moved up by a certain number of lines? */
  frame.y = 0;

  if ( initialized ) {
    int lines_scrolled = 0;
    int scroll_height = 0;

    for ( int row = 0; row < f.ds.get_height(); row++ ) {
      if ( *(f.get_row( 0 )) == *(frame.last_frame.get_row( row )) ) {
	/* found a scroll */
	lines_scrolled = row;
	scroll_height = 1;

	/* how big is the region that was scrolled? */
	for ( int region_height = 1;
	      lines_scrolled + region_height < f.ds.get_height();
	      region_height++ ) {
	  if ( *(f.get_row( region_height ))
	       == *(frame.last_frame.get_row( lines_scrolled + region_height )) ) {
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
	if ( !(frame.current_rendition == initial_rendition()) ) {
	  frame.append( "\033[0m" );
	  frame.current_rendition = initial_rendition();
	}

	int top_margin = 0;
	int bottom_margin = top_margin + lines_scrolled + scroll_height - 1;

	assert( bottom_margin < f.ds.get_height() );

	/* set scrolling region */
	snprintf( tmp, 64, "\033[%d;%dr",
		  top_margin + 1, bottom_margin + 1);
	frame.append( tmp );

	/* go to bottom of scrolling region */
	frame.append_silent_move( bottom_margin, 0 );

	/* scroll */
	for ( int i = 0; i < lines_scrolled; i++ ) {
	  frame.append( "\n" );
	}

	/* do the move in memory */
	for ( int i = top_margin; i <= bottom_margin; i++ ) {
	  if ( i + lines_scrolled <= bottom_margin ) {
	    *(frame.last_frame.get_mutable_row( i )) = *(frame.last_frame.get_row( i + lines_scrolled ));
	  } else {
	    frame.last_frame.get_mutable_row( i )->reset( 0 );
	  }
	}

	/* reset scrolling region */
	snprintf( tmp, 64, "\033[%d;%dr",
		  1, f.ds.get_height() );
	frame.append( tmp );

	/* invalidate cursor position after unsetting scrolling region */
	frame.cursor_x = frame.cursor_y = -1;
      }
    }
  }

  /* iterate for every cell */
  for ( ; frame.y < f.ds.get_height(); frame.y++ ) {
    int last_x = 0;
    for ( frame.x = 0;
	  frame.x < f.ds.get_width(); /* let put_cell() handle advance */ ) {
      last_x = frame.x;
      put_cell( initialized, frame, f );
    }

    /* To hint that a word-select should group the end of one line
       with the beginning of the next, we let the real cursor
       actually wrap around in cases where it wrapped around for us. */

    if ( (frame.y < f.ds.get_height() - 1)
	 && f.get_row( frame.y )->get_wrap() ) {
      frame.x = last_x;

      while ( frame.x < f.ds.get_width() ) {
	frame.force_next_put = true;
	put_cell( initialized, frame, f );
      }

      /* next write will wrap */
      frame.cursor_x = 0;
      frame.cursor_y++;
      frame.force_next_put = true;
    }

    /* Turn off wrap */
    if ( (frame.y < f.ds.get_height() - 1)
	 && (!f.get_row( frame.y )->get_wrap())
	 && (!initialized || frame.last_frame.get_row( frame.y )->get_wrap()) ) {
      frame.x = last_x;
      if ( initialized ) {
	frame.last_frame.reset_cell( frame.last_frame.get_mutable_cell( frame.y, frame.x ) );
      }

      snprintf( tmp, 64, "\033[%d;%dH\033[K", frame.y + 1, frame.x + 1 );
      frame.append( tmp );
      frame.cursor_x = frame.x;

      frame.force_next_put = true;
      put_cell( initialized, frame, f );
    }
  }

  /* has cursor location changed? */
  if ( (!initialized)
       || (f.ds.get_cursor_row() != frame.cursor_y)
       || (f.ds.get_cursor_col() != frame.cursor_x) ) {
    snprintf( tmp, 64, "\033[%d;%dH", f.ds.get_cursor_row() + 1,
	      f.ds.get_cursor_col() + 1 );
    frame.append( tmp );
    frame.cursor_x = f.ds.get_cursor_col();
    frame.cursor_y = f.ds.get_cursor_row();
  }

  /* has cursor visibility changed? */
  if ( (!initialized)
       || (f.ds.cursor_visible != frame.last_frame.ds.cursor_visible) ) {
    if ( f.ds.cursor_visible ) {
      frame.append( "\033[?25h" );
    } else {
      frame.append( "\033[?25l" );
    }
  }

  /* have renditions changed? */
  if ( (!initialized)
       || !(f.ds.get_renditions() == frame.current_rendition) ) {
    frame.appendstring( f.ds.get_renditions().sgr() );
    frame.current_rendition = f.ds.get_renditions();
  }

  return frame.str;
}

void Display::put_cell( bool initialized, FrameState &frame, const Framebuffer &f ) const
{
  char tmp[ 64 ];

  const Cell *cell = f.get_cell( frame.y, frame.x );

  if ( !frame.force_next_put ) {
    if ( initialized
	 && ( *cell == *(frame.last_frame.get_cell( frame.y, frame.x )) ) ) {
      frame.x += cell->width;
      return;
    }
  }

  if ( (frame.x != frame.cursor_x) || (frame.y != frame.cursor_y) ) {
    frame.append_silent_move( frame.y, frame.x );
  }

  if ( !(frame.current_rendition == cell->renditions) ) {
    /* print renditions */
    frame.appendstring( cell->renditions.sgr() );
    frame.current_rendition = cell->renditions;
  }

  if ( cell->contents.empty() ) {
    /* see how far we can stretch a clear */
    int clear_count = 0;
    for ( int col = frame.x; col < f.ds.get_width(); col++ ) {
      const Cell *other_cell = f.get_cell( frame.y, col );
      if ( (cell->renditions == other_cell->renditions)
	   && (other_cell->contents.empty()) ) {
	clear_count++;
      } else {
	break;
      }
    }

    assert( frame.x + clear_count <= f.ds.get_width() );

    bool can_use_erase = has_bce || (cell->renditions == initial_rendition());

    if ( frame.force_next_put ) {
      frame.append( " " );
      frame.cursor_x++;
      frame.x++;
      frame.force_next_put = false;
      return;
    }

    /* can we go to the end of the line? */
    if ( (frame.x + clear_count == f.ds.get_width())
	 && can_use_erase ) {
      frame.append( "\033[K" );
      frame.x += clear_count;
    } else {
      if ( has_ech && can_use_erase ) {
	if ( clear_count == 1 ) {
	  frame.append( "\033[X" );
	} else {
	  snprintf( tmp, 64, "\033[%dX", clear_count );
	  frame.append( tmp );
	}
	frame.x += clear_count;
      } else { /* no ECH, so just print a space */
	/* unlike erases, this will use background color irrespective of BCE */
	frame.append( " " );
	frame.cursor_x++;
	frame.x++;
      }
    }

    return;
  }

  /* cells that begin with combining character get combiner attached to no-break space */
  if ( cell->fallback ) {
    frame.append( "\xC2\xA0" );
  }

  for ( std::vector<wchar_t>::const_iterator i = cell->contents.begin();
	i != cell->contents.end();
	i++ ) {
    snprintf( tmp, 64, "%lc", *i );
    frame.append( tmp );
  }

  frame.x += cell->width;
  frame.cursor_x += cell->width;

  frame.force_next_put = false;
}

void FrameState::append_silent_move( int y, int x )
{
  char tmp[ 64 ];

  /* turn off cursor if necessary before moving cursor */
  if ( last_frame.ds.cursor_visible ) {
    append( "\033[?25l" );
    last_frame.ds.cursor_visible = false;
  }

  snprintf( tmp, 64, "\033[%d;%dH", y + 1, x + 1 );
  append( tmp );
  cursor_x = x;
  cursor_y = y;
}
