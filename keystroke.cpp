#include <assert.h>

#include "keystroke.hpp"

void KeyStroke::subtract( KeyStroke * const prefix )
{
  for ( deque<char>::iterator i = prefix->user_bytes.begin();
	i != prefix->user_bytes.end();
	i++ ) {
    assert( *i == user_bytes.front() );
    user_bytes.pop_front();
  }
}

string KeyStroke::diff_from( KeyStroke const & existing, int length_limit )
{
  string ret;

  deque<char>::iterator my_it = user_bytes.begin();

  for ( deque<char>::const_iterator i = existing.user_bytes.begin();
	i != existing.user_bytes.end();
	i++ ) {
    assert( *i == *my_it );
    my_it++;
  }

  while ( (my_it != user_bytes.end())
	  && ( (length_limit < 0) ? true : (int(ret.size()) < length_limit) ) ) {
    ret += string( &( *my_it ), 1 );
    my_it++;
  }

  return ret;
}

void KeyStroke::apply_string( string diff )
{
  for ( string::iterator i = diff.begin();
	i != diff.end();
	i++ ) {
    user_bytes.push_back( *i );
  }
}
