#include <list>
#include <vector>
#include <deque>
#include <wchar.h>
#include <string>

#include "terminal.hpp"

namespace Parser {
  class Action;
}

using namespace std;
using namespace Terminal;

template class list<Parser::Action *>;
template class vector<Cell>;
template class deque<Row>;
template class vector<Cell *>;
template class vector<wchar_t>;
template class vector<int>;
template class map<string, Function>;
template class vector<bool>;
template class list<int>;
template void std::list<int, std::allocator<int> >::remove_if<bool (*)(int const&)>(bool (*)(int const&));
