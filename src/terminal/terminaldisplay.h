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

#ifndef TERMINALDISPLAY_HPP
#define TERMINALDISPLAY_HPP

#include "terminalframebuffer.h"

namespace Terminal {
  /* variables used within a new_frame */
  class FrameState {
  public:
    int x, y;
    bool force_next_put;
    std::string str;

    int cursor_x, cursor_y;
    Renditions current_rendition;

    Framebuffer last_frame;

    FrameState( const Framebuffer &s_last )
      : x(0), y(0),
	force_next_put( false ),
	str(), cursor_x(0), cursor_y(0), current_rendition( 0 ),
	last_frame( s_last )
    {
      str.reserve( 1024 );
    }

    void append( const char * s ) { str.append( s ); }
    void appendstring( const std::string s ) { str.append( s ); }

    void append_silent_move( int y, int x );
  };

  class Display {
  private:
    bool ti_flag( const char *capname ) const;
    int ti_num( const char *capname ) const;
    const char *ti_str( const char *capname ) const;

    bool has_ech; /* erase character is part of vt200 but not supported by tmux
		     (or by "screen" terminfo entry, which is what tmux advertises) */

    bool has_bce; /* erases result in cell filled with background color */

    bool has_title; /* supports window title and icon name */

    int posterize_colors; /* downsample input colors >8 to [0..7] */

    void put_cell( bool initialized, FrameState &frame, const Framebuffer &f ) const;

  public:
    void downgrade( Framebuffer &f ) const { if ( posterize_colors ) { f.posterize(); } }

    std::string open() const;
    std::string close() const;

    std::string new_frame( bool initialized, const Framebuffer &last, const Framebuffer &f ) const;

    Display( bool use_environment );
  };
}

#endif
