/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <wctype.h>

#include "parseraction.h"
#include "terminal.h"

using namespace Parser;

std::string Action::str( void )
{
  char thechar[ 10 ] = { 0 };
  if ( char_present ) {
    snprintf( thechar, 10, iswprint( ch ) ? "(%lc)" : "(0x%x)", ch );
  }

  return name() + std::string( thechar );
}

void Print::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->print( this );
}

void Execute::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->execute( this );
}

void Clear::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->dispatch.clear( this );
}

void Param::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->dispatch.newparamchar( this );
}

void Collect::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->dispatch.collect( this );
}

void CSI_Dispatch::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->CSI_dispatch( this );
}

void Esc_Dispatch::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->Esc_dispatch( this );
}

void OSC_Put::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->dispatch.OSC_put( this );
}

void OSC_Start::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->dispatch.OSC_start( this );
}

void OSC_End::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->OSC_end( this );
}

void UserByte::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->dispatch.terminal_to_host.append( emu->user.input( this,
							  emu->fb.ds.application_mode_cursor_keys ) );
}

void Resize::act_on_terminal( Terminal::Emulator *emu ) const
{
  emu->resize( width, height );
  handled = true;
}

bool Action::operator==( const Action &other ) const
{
  return ( char_present == other.char_present )
    && ( ch == other.ch )
    && ( handled == other.handled );
}
