#ifndef TERMINALDISPLAY_HPP
#define TERMINALDISPLAY_HPP

#include "terminalframebuffer.hpp"

namespace Terminal {
  class Display {
  private:
    bool initialized;
    Framebuffer last_frame;
    std::string current_rendition_string;
    int cursor_x, cursor_y;
    int x, y;
    std::string str;

    void put_cell( Framebuffer &f );

  public:
    Display( int width, int height )
      : initialized( false ), last_frame( width, height ),
	current_rendition_string(), cursor_x( -1 ), cursor_y( -1 ),
	x( 0 ), y( 0 ), str( "" )
    {}

    std::string new_frame( Framebuffer &f );
    void invalidate( void ) { initialized = false; }
  };
}

#endif
