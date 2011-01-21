#include <vector>
#include <wchar.h>

#include "terminal.hpp"

namespace Parser {
  class Action;
}

template class std::vector<Parser::Action *>;
template class std::vector<Terminal::Cell>;
template class std::vector<Terminal::Row>;
template class std::vector<Terminal::Cell *>;
template class std::vector<wchar_t>;
