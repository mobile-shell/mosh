#include "parserstate.hpp"

using namespace Parser;

const Transition State::anywhere_rule( wchar_t ch )
{
  if ( (ch == 0x18) || (ch == 0x1A)
       || ((0x80 <= ch) && (ch <= 0x8F))
       || ((0x91 <= ch) && (ch <= 0x97))
       || (ch == 0x99) || (ch == 0x9A) ) {
    return Transition( Execute(), new Ground() );
  } else if ( ch == 0x9C ) {
    return Transition( Ignore(), new Ground() );
  } else if ( ch == 0x1B ) {
    return Transition( Ignore(), new Escape() );
  } else if ( (ch == 0x98) || (ch == 0x9E) || (ch == 0x9F) ) {
    return Transition( Ignore(), new SOS_PM_APC_String() );
  } else if ( ch == 0x90 ) {
    return Transition( Ignore(), new DCS_Entry() );
  } else if ( ch == 0x9D ) {
    return Transition( Ignore(), new OSC_String() );
  } else if ( ch == 0x9B ) {
    return Transition( Ignore(), new CSI_Entry() );
  }

  return Transition( Ignore(), NULL );
}

const Transition State::input( wchar_t ch )
{
  Transition any = anywhere_rule( ch );
  if ( any.next_state ) {
    return any;
  } else {
    return this->input_state_rule( ch );
  }
}
