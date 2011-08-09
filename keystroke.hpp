#ifndef KEYSTROKE_HPP
#define KEYSTROKE_HPP

#include <deque>
#include <string>
#include <assert.h>

using namespace std;

class KeyStroke
{
public:
  deque<char> user_bytes;

  KeyStroke() : user_bytes() {}

  void key_hit( char x ) { user_bytes.push_back( x ); }

  /* interface for Network::Transport */
  void subtract( KeyStroke * const prefix );
  string diff_from( KeyStroke const & existing, int length_limit );
  void apply_string( string diff );
  bool operator==( KeyStroke const &x ) const { return user_bytes == x.user_bytes; }
};

#endif
