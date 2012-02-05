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
