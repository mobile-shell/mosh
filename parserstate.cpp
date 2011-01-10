#include "parserstate.hpp"
#include "parserstatefamily.hpp"

using namespace Parser;

Transition State::anywhere_rule( wchar_t ch )
{
  if ( (ch == 0x18) || (ch == 0x1A)
       || ((0x80 <= ch) && (ch <= 0x8F))
       || ((0x91 <= ch) && (ch <= 0x97))
       || (ch == 0x99) || (ch == 0x9A) ) {
    return Transition( Execute(), &family->s_Ground );
  } else if ( ch == 0x9C ) {
    return Transition( Ignore(), &family->s_Ground );
  } else if ( ch == 0x1B ) {
    return Transition( Ignore(), &family->s_Escape );
  } else if ( (ch == 0x98) || (ch == 0x9E) || (ch == 0x9F) ) {
    return Transition( Ignore(), &family->s_SOS_PM_APC_String );
  } else if ( ch == 0x90 ) {
    return Transition( Ignore(), &family->s_DCS_Entry );
  } else if ( ch == 0x9D ) {
    return Transition( Ignore(), &family->s_OSC_String );
  } else if ( ch == 0x9B ) {
    return Transition( Ignore(), &family->s_CSI_Entry );
  }

  return Transition( Ignore(), NULL );
}

Transition State::input( wchar_t ch )
{
  Transition any = anywhere_rule( ch );
  if ( any.next_state ) {
    return any;
  }

  Transition ret;

  if ( ch >= 0xA0 ) {
    ret = this->input_state_rule( 0x41 );
  } else {
    ret = this->input_state_rule( ch );
  }

  ret.action.ch = ch;
  return ret;
}

static bool C0_prime( wchar_t ch )
{
  return ( (ch <= 0x17)
	   || (ch == 0x19)
	   || ( (0x1C <= ch) && (ch <= 0x1F) ) );
}

static bool GLGR ( wchar_t ch )
{
  return ( ( (0x20 <= ch) && (ch <= 0x7F) ) /* GL area */
	   || ( (0xA0 <= ch) && (ch <= 0xFF) ) ); /* GR area */
}

Transition Ground::input_state_rule( wchar_t ch )
{
  if ( C0_prime( ch ) ) {
    return Transition( Execute(), NULL );
  }

  if ( GLGR( ch ) ) {
    return Transition( Print(), NULL );
  }

  return Transition( Ignore(), NULL );
}

Action Escape::enter( void )
{
  return Clear();
}

Transition Escape::input_state_rule( wchar_t ch )
{
  if ( C0_prime( ch ) ) {
    return Transition( Execute(), NULL );
  }

  if ( (0x20 <= ch) && (ch <= 0x2F) ) {
    return Transition( Collect(), &family->s_Escape_Intermediate );
  }

  if ( ( (0x30 <= ch) && (ch <= 0x4F) )
       || ( (0x51 <= ch) && (ch <= 0x57) )
       || ( ch == 0x59 )
       || ( ch == 0x5A )
       || ( ch == 0x5C )
       || ( (0x60 <= ch) && (ch <= 0x7E) ) ) {
    return Transition( Esc_Dispatch(), &family->s_Ground );
  }

  if ( ch == 0x5B ) {
    return Transition( Ignore(), &family->s_CSI_Entry );
  }

  if ( ch == 0x5D ) {
    return Transition( Ignore(), &family->s_OSC_String );
  }

  if ( ch == 0x50 ) {
    return Transition( Ignore(), &family->s_DCS_Entry );
  }

  if ( (ch == 0x58) || (ch == 0x5E) || (ch == 0x5F) ) {
    return Transition( Ignore(), &family->s_SOS_PM_APC_String );
  }

  return Transition( Ignore(), NULL );
}

Transition Escape_Intermediate::input_state_rule( wchar_t ch )
{
  if ( C0_prime( ch ) ) {
    return Transition( Execute(), NULL );
  }

  if ( (0x20 <= ch) && (ch <= 0x2F) ) {
    return Transition( Collect(), NULL );
  }

  if ( (0x30 <= ch) && (ch <= 0x7E) ) {
    return Transition( Esc_Dispatch(), &family->s_Ground );
  }

  return Transition( Ignore(), NULL );
}

Action CSI_Entry::enter( void )
{
  return Clear();
}

Transition CSI_Entry::input_state_rule( wchar_t ch )
{
  if ( C0_prime( ch ) ) {
    return Transition( Execute(), NULL );
  }

  if ( (0x40 <= ch) && (ch <= 0x7E) ) {
    return Transition( CSI_Dispatch(), &family->s_Ground );
  }

  if ( ( (0x30 <= ch) && (ch <= 0x39) )
       || ( ch == 0x3B ) ) {
    return Transition( Param(), &family->s_CSI_Param );
  }

  if ( (ch <= 0x3C) && (ch <= 0x3F) ) {
    return Transition( Collect(), &family->s_CSI_Param );
  }

  if ( ch == 0x3A ) {
    return Transition( Ignore(), &family->s_CSI_Ignore );
  }

  if ( (0x20 <= ch) && (ch <= 0x2F) ) {
    return Transition( Collect(), &family->s_CSI_Intermediate );
  }

  return Transition( Ignore(), NULL );
}

Transition CSI_Param::input_state_rule( wchar_t ch )
{
  if ( ( (0x30 <= ch) && (ch <= 0x39) ) || ( ch == 0x3B ) ) {
    return Transition( Param(), NULL );
  }

  if ( ( ch == 0x3A ) || ( (0x3C <= ch) && (ch <= 0x3F) ) ) {
    return Transition( Ignore(), &family->s_CSI_Ignore );
  }

  if ( (0x20 <= ch) && (ch <= 0x2F) ) {
    return Transition( Collect(), &family->s_CSI_Intermediate );
  }

  if ( (0x40 <= ch) && (ch <= 0x7E) ) {
    return Transition( CSI_Dispatch(), &family->s_Ground );
  }

  return Transition( Ignore(), NULL );
}

Transition CSI_Intermediate::input_state_rule( wchar_t ch )
{
  if ( C0_prime( ch ) ) {
    return Transition( Execute(), NULL );
  }

  if ( (0x20 <= ch) && (ch <= 0x2F) ) {
    return Transition( Collect(), NULL );
  }

  if ( (0x40 <= ch) && (ch <= 0x7E) ) {
    return Transition( CSI_Dispatch(), &family->s_Ground );
  }

  if ( (0x30 <= ch) && (ch <= 0x3F) ) {
    return Transition( Ignore(), &family->s_CSI_Ignore );
  }

  return Transition( Ignore(), NULL );
}

Transition CSI_Ignore::input_state_rule( wchar_t ch )
{
  if ( C0_prime( ch ) ) {
    return Transition( Execute(), NULL );
  }

  if ( (0x40 <= ch) && (ch <= 0x7E) ) {
    return Transition( Ignore(), &family->s_Ground );
  }

  return Transition( Ignore(), NULL );
}

Action DCS_Entry::enter( void )
{
  return Clear();
}

Transition DCS_Entry::input_state_rule( wchar_t ch )
{
  if ( (0x20 <= ch) && (ch <= 0x2F) ) {
    return Transition( Collect(), &family->s_DCS_Intermediate );
  }

  if ( ch == 0x3A ) {
    return Transition( Ignore(), &family->s_DCS_Ignore );
  }

  if ( ( (0x30 <= ch) && (ch <= 0x39) ) || ( ch == 0x3B ) ) {
    return Transition( Param(), &family->s_DCS_Param );
  }

  if ( (0x3C <= ch) && (ch <= 0x3F) ) {
    return Transition( Collect(), &family->s_DCS_Param );
  }

  if ( (0x40 <= ch) && (ch <= 0x7E) ) {
    return Transition( Ignore(), &family->s_DCS_Passthrough );
  }

  return Transition( Ignore(), NULL );
}

Transition DCS_Param::input_state_rule( wchar_t ch )
{
  if ( ( (0x30 <= ch) && (ch <= 0x39) ) || ( ch == 0x3B ) ) {
    return Transition( Param(), NULL );
  }

  if ( ( ch == 0x3A ) || ( (0x3C <= ch) && (ch <= 0x3F) ) ) {
    return Transition( Ignore(), &family->s_DCS_Ignore );
  }

  if ( (0x20 <= ch) && (ch <= 0x2F) ) {
    return Transition( Collect(), &family->s_DCS_Intermediate );
  }

  if ( (0x40 <= ch) && (ch <= 0x7E) ) {
    return Transition( Ignore(), &family->s_DCS_Passthrough );
  }

  return Transition( Ignore(), NULL );
}

Transition DCS_Intermediate::input_state_rule( wchar_t ch )
{
  if ( (0x20 <= ch) && (ch <= 0x2F) ) {
    return Transition( Collect(), NULL );
  }

  if ( (0x40 <= ch) && (ch <= 0x7E) ) {
    return Transition( Ignore(), &family->s_DCS_Passthrough );
  }

  if ( (0x30 <= ch) && (ch <= 0x3F) ) {
    return Transition( Ignore(), &family->s_DCS_Ignore );
  }

  return Transition( Ignore(), NULL );
}

Action DCS_Passthrough::enter( void )
{
  return Hook();
}

Action DCS_Passthrough::exit( void )
{
  return Unhook();
}

Transition DCS_Passthrough::input_state_rule( wchar_t ch )
{
  if ( C0_prime( ch ) || ( (0x20 <= ch) && (ch <= 0x7E) ) ) {
    return Transition( Put(), NULL );
  }

  if ( ch == 0x9C ) {
    return Transition( Ignore(), &family->s_Ground );
  }

  return Transition( Ignore(), NULL );
}

Transition DCS_Ignore::input_state_rule( wchar_t ch )
{
  if ( ch == 0x9C ) {
    return Transition( Ignore(), &family->s_Ground );
  }

  return Transition( Ignore(), NULL );
}

Action OSC_String::enter( void )
{
  return OSC_Start();
}

Action OSC_String::exit( void )
{
  return OSC_End();
}

Transition OSC_String::input_state_rule( wchar_t ch )
{
  if ( (0x20 <= ch) && (ch <= 0x7F) ) {
    return Transition( OSC_Put(), NULL );
  }

  if ( ch == 0x9C ) {
    return Transition( Ignore(), &family->s_Ground );
  }

  return Transition( Ignore(), NULL );
}

Transition SOS_PM_APC_String::input_state_rule( wchar_t ch )
{
  if ( ch == 0x9C ) {
    return Transition( Ignore(), &family->s_Ground );
  }

  return Transition( Ignore(), NULL );
}
