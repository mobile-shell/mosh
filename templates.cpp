#include <list>
#include <vector>
#include <deque>
#include <wchar.h>
#include <string>

#include "terminal.hpp"

#include "keystroke.hpp"
#include "networktransport.cpp"

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
template class deque<char>;

template class Network::Transport<KeyStroke, KeyStroke>;
