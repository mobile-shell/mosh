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

void Clear::act_on_terminal( Terminal::Emulator *emu )
{
  emu->clear();
}

void Param::act_on_terminal( Terminal::Emulator *emu )
{
  emu->param( this );
}

void CSI_Dispatch::act_on_terminal( Terminal::Emulator *emu )
{
  emu->CSI_dispatch( this );
}
