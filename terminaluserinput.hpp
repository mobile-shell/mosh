#ifndef TERMINALUSERINPUT_HPP
#define TERMINALUSERINPUT_HPP

#include <string>
#include "parseraction.hpp"

namespace Terminal {
  class UserInput {
  private:
    bool last_byte;
    bool application_mode_cursor;

  public:
    UserInput()
      : last_byte( 0 ),
	application_mode_cursor( false )
    {}

    std::string input( Parser::UserByte *act );
    void set_cursor_application_mode( bool mode ) { application_mode_cursor = mode; }
  };
}

#endif
