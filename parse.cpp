#include <vector>
#include <iostream>

#include "parser.hpp"

int main( void )
{
  Parser::Parser parser;

  std::vector<Parser::Action> a, b, c;

  a = parser.input( 'x' );
  b = parser.input( 'y' );
  c = parser.input( 'z' );

  std::cout << a[0].name;

  return 0;
}
