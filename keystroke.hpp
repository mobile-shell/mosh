#ifndef KEYSTROKE_HPP
#define KEYSTROKE_HPP

#include <string>
#include <assert.h>

class KeyStroke
{
public:
  char letter;

  string tostring( void )
  {
    return string( &letter, 1 );
  };

  KeyStroke( const string x )
    : letter()
  {
    assert( x.size() == 1 );

    letter = x[ 0 ];
  }

  KeyStroke()
    : letter( 0 )
  {}
};

#endif
