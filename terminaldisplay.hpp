#ifndef TERMINALDISPLAY_HPP
#define TERMINALDISPLAY_HPP

#include "terminalframebuffer.hpp"

namespace Terminal {
  /* variables used within a new_frame */
  class FrameState {
  public:
    int x, y;
    std::string str;

    int cursor_x, cursor_y;
    std::string current_rendition_string;

    Framebuffer last_frame;

    FrameState( const Framebuffer &s_last )
      : x(0), y(0),
	str(), cursor_x(0), cursor_y(0), current_rendition_string(),
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
    static void put_cell( bool initialized, FrameState &frame, const Framebuffer &f );

  public:
    static std::string new_frame( bool initialized, const Framebuffer &last, const Framebuffer &f );
  };
}

#endif
