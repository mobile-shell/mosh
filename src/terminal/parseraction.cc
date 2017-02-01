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

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#include <stdio.h>
#include <wctype.h>

#include "parseraction.h"
#include "terminal.h"

using namespace Parser;

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
}

bool Action::operator==( const Action &other ) const
{
  return ( char_present == other.char_present )
    && ( ch == other.ch );
}
