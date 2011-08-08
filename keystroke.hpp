#ifndef KEYSTROKE_HPP
#define KEYSTROKE_HPP

#include <string>
#include <assert.h>

using namespace std;

class KeyStroke
{
public:
  void subtract( KeyStroke * const ) {}
  string diff_from( KeyStroke const &, int ) { return ""; }
  void apply_string( string ) {}
  bool operator==( KeyStroke const & ) const { return true; }
};

#endif
