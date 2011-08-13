#include <list>
#include <vector>
#include <deque>
#include <wchar.h>
#include <string>

#include "terminal.hpp"
#include "completeterminal.hpp"

#include "user.hpp"
#include "networktransport.cpp"
#include "userinput.pb.h"

namespace Parser {
  class Action;
}

using namespace std;
using namespace Terminal;
using namespace Network;

template class list<Parser::Action *>;
template class vector<Cell>;
template class deque<Row>;
template class vector<Cell *>;
template class vector<wchar_t>;
template class vector<int>;
template class map<string, Function>;
template class vector<bool>;

template class vector<Instruction>;
template class Transport<UserStream, UserStream>;
template class Transport<Complete, UserStream>;
template class Transport<UserStream, Complete>;

template class deque<UserEvent>;
