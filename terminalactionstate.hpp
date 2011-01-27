#ifndef TERMINALACTIONSTATE_HPP
#define TERMINALACTIONSTATE_HPP

#include <vector>

namespace Parser {
  class Param;
  class Collect;
  class Clear;
}

namespace Terminal {
  class ActionState {
  private:
    std::string params;
    std::vector<int> parsed_params;
    bool parsed;

    std::string dispatch_chars;

    void parse_params( void );

  public:
    ActionState();
    int getparam( size_t N, int defaultval );

    void newparamchar( Parser::Param *act );
    void collect( Parser::Collect *act );
    void clear( Parser::Clear *act );
    
    std::string str( void );
  };
}

#endif
