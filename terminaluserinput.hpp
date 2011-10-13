#ifndef TERMINALUSERINPUT_HPP
#define TERMINALUSERINPUT_HPP

#include <string>
#include "parseraction.hpp"

namespace Terminal {
  class UserInput {
  private:
    wchar_t last_byte;

  public:
    UserInput()
      : last_byte( -1 )
    {}

    std::string input( const Parser::UserByte *act,
		       bool application_mode_cursor_keys );

    bool operator==( const UserInput &x ) const { return last_byte == x.last_byte; }
  };
}

#endif
