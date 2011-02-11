#ifndef TERMINALDISPLAY_HPP
#define TERMINALDISPLAY_HPP

#include "terminalframebuffer.hpp"

namespace Terminal {
  /* variables used within a new_frame */
  class FrameState {
  public:
    int x, y;
    std::string str;

    FrameState() : x(0), y(0), str() {}
    void append( std::string s ) { str.append( s ); }
  };

  class Display {
  private:
    bool initialized;
    Framebuffer last_frame;
    std::string current_rendition_string;
    int cursor_x, cursor_y;

    void put_cell( FrameState &frame, Framebuffer &f );

  public:
    Display( int width, int height )
      : initialized( false ), last_frame( width, height ),
	current_rendition_string(), cursor_x( -1 ), cursor_y( -1 )
    {}

    std::string new_frame( Framebuffer &f );
    void invalidate( void ) { initialized = false; }
  };
}

#endif
