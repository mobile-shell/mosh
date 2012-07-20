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

#ifndef PARSERSTATEFAMILY_HPP
#define PARSERSTATEFAMILY_HPP

#include "parserstate.h"

namespace Parser {
  class StateFamily
  {
  public:
    Ground s_Ground;

    Escape s_Escape;
    Escape_Intermediate s_Escape_Intermediate;

    CSI_Entry s_CSI_Entry;
    CSI_Param s_CSI_Param;
    CSI_Intermediate s_CSI_Intermediate;
    CSI_Ignore s_CSI_Ignore;

    DCS_Entry s_DCS_Entry;
    DCS_Param s_DCS_Param;
    DCS_Intermediate s_DCS_Intermediate;
    DCS_Passthrough s_DCS_Passthrough;
    DCS_Ignore s_DCS_Ignore;

    OSC_String s_OSC_String;
    SOS_PM_APC_String s_SOS_PM_APC_String;

    StateFamily()
      : s_Ground(), s_Escape(), s_Escape_Intermediate(),
	s_CSI_Entry(), s_CSI_Param(), s_CSI_Intermediate(), s_CSI_Ignore(),
	s_DCS_Entry(), s_DCS_Param(), s_DCS_Intermediate(),
	s_DCS_Passthrough(), s_DCS_Ignore(),
	s_OSC_String(), s_SOS_PM_APC_String()
    {
      s_Ground.setfamily( this );
      s_Escape.setfamily( this );
      s_Escape_Intermediate.setfamily( this );
      s_CSI_Entry.setfamily( this );
      s_CSI_Param.setfamily( this );
      s_CSI_Intermediate.setfamily( this );
      s_CSI_Ignore.setfamily( this );
      s_DCS_Entry.setfamily( this );
      s_DCS_Param.setfamily( this );
      s_DCS_Intermediate.setfamily( this );
      s_DCS_Passthrough.setfamily( this );
      s_DCS_Ignore.setfamily( this );
      s_OSC_String.setfamily( this );
      s_SOS_PM_APC_String.setfamily( this );
    }
  };
}

#endif
