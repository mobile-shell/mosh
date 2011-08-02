#include "network.hpp"

class KeyStroke
{
public:
  char letter;
};

int main( void )
{
  Network::Connection<KeyStroke> n();
}
