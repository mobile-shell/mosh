#include <vector>

#include "parser.hpp"

int main( void )
{
  Parser::Parser parser;

  std::vector<Parser::Action> a, b, c;

  a = parser.input( 'x' );
  b = parser.input( 'y' );
  c = parser.input( 'z' );

  return 0;
}
