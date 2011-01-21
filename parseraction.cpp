#include "parseraction.hpp"
#include "terminal.hpp"

using namespace Parser;

void Print::act_on_terminal( Terminal::Emulator *emu )
{
  emu->print( this );
}

void Execute::act_on_terminal( Terminal::Emulator *emu )
{
  emu->execute( this );
}
