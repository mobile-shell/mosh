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
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "terminaldispatcher.h"
#include "parseraction.h"
#include "terminalframebuffer.h"

using namespace Terminal;

Dispatcher::Dispatcher()
  : params(), parsed_params(), parsed( false ), dispatch_chars(),
    OSC_string(), terminal_to_host()
{}

void Dispatcher::newparamchar( const Parser::Param *act )
{
  assert( act->char_present );
  assert( (act->ch == ';') || ( (act->ch >= '0') && (act->ch <= '9') ) );
  if ( params.length() < 100 ) {
    /* enough for 16 five-char params plus 15 semicolons */
    params.push_back( act->ch );
  }
  parsed = false;
}

void Dispatcher::collect( const Parser::Collect *act )
{
  assert( act->char_present );
  if ( ( dispatch_chars.length() < 8 ) /* never should need more than 2 */
       && ( act->ch <= 255 ) ) {  /* ignore non-8-bit */    
    dispatch_chars.push_back( act->ch );
  }
}

void Dispatcher::clear( const Parser::Clear *act __attribute((unused)) )
{
  params.clear();
  dispatch_chars.clear();
  parsed = false;
}

void Dispatcher::parse_params( void )
{
  if ( parsed ) {
    return;
  }

  parsed_params.clear();
  const char *str = params.c_str();
  const char *segment_begin = str;

  while ( 1 ) {
    const char *segment_end = strchr( segment_begin, ';' );
    if ( segment_end == NULL ) {
      break;
    }

    errno = 0;
    char *endptr;
    long val = strtol( segment_begin, &endptr, 10 );
    if ( endptr == segment_begin ) {
      val = -1;
    }

    if ( val > PARAM_MAX || errno == ERANGE ) {
      val = -1;
      errno = 0;
    }

    if ( errno == 0 || segment_begin == endptr ) {
      parsed_params.push_back( val );
    }

    segment_begin = segment_end + 1;
  }

  /* get last param */
  errno = 0;
  char *endptr;
  long val = strtol( segment_begin, &endptr, 10 );
  if ( endptr == segment_begin ) {
    val = -1;
  }

  if ( val > PARAM_MAX || errno == ERANGE ) {
    val = -1;
    errno = 0;
  }

  if ( errno == 0 || segment_begin == endptr ) {
    parsed_params.push_back( val );
  }

  parsed = true;
}

int Dispatcher::getparam( size_t N, int defaultval )
{
  int ret = defaultval;
  if ( !parsed ) {
    parse_params();
  }

  if ( parsed_params.size() > N ) {
    ret = parsed_params[ N ];
  }

  if ( ret < 1 ) ret = defaultval;

  return ret;
}

int Dispatcher::param_count( void )
{
  if ( !parsed ) {
    parse_params();
  }

  return parsed_params.size();
}

std::string Dispatcher::str( void )
{
  char assum[ 64 ];
  snprintf( assum, 64, "[dispatch=\"%s\" params=\"%s\"]",
	    dispatch_chars.c_str(), params.c_str() );
  return std::string( assum );
}

/* construct on first use to avoid static initialization order crash */
DispatchRegistry & Terminal::get_global_dispatch_registry( void )
{
  static DispatchRegistry global_dispatch_registry;
  return global_dispatch_registry;
}

static void register_function( Function_Type type,
			       const std::string & dispatch_chars,
			       Function f )
{
  switch ( type ) {
  case ESCAPE:
    get_global_dispatch_registry().escape.insert( dispatch_map_t::value_type( dispatch_chars, f ) );
    break;
  case CSI:
    get_global_dispatch_registry().CSI.insert( dispatch_map_t::value_type( dispatch_chars, f ) );
    break;
  case CONTROL:
    get_global_dispatch_registry().control.insert( dispatch_map_t::value_type( dispatch_chars, f ) );
    break;
  }
}

Function::Function( Function_Type type, const std::string & dispatch_chars,
		    void (*s_function)( Framebuffer *, Dispatcher * ),
		    bool s_clears_wrap_state )
  : function( s_function ), clears_wrap_state( s_clears_wrap_state )
{
  register_function( type, dispatch_chars, *this );
}

void Dispatcher::dispatch( Function_Type type, const Parser::Action *act, Framebuffer *fb )
{
  /* add final char to dispatch key */
  if ( (type == ESCAPE) || (type == CSI) ) {
    assert( act->char_present );
    Parser::Collect act2;
    act2.char_present = true;
    act2.ch = act->ch;
    collect( &act2 ); 
  }

  dispatch_map_t *map = NULL;
  switch ( type ) {
  case ESCAPE:  map = &get_global_dispatch_registry().escape;  break;
  case CSI:     map = &get_global_dispatch_registry().CSI;     break;
  case CONTROL: map = &get_global_dispatch_registry().control; break;
  }

  std::string key = dispatch_chars;
  if ( type == CONTROL ) {
    assert( act->ch <= 255 );
    char ctrlstr[ 2 ] = { (char)act->ch, 0 };
    key = std::string( ctrlstr, 1 );
  }

  dispatch_map_t::const_iterator i = map->find( key );
  if ( i == map->end() ) {
    /* unknown function */
    fb->ds.next_print_will_wrap = false;
    return;
  } else {
    if ( i->second.clears_wrap_state ) {
      fb->ds.next_print_will_wrap = false;
    }
    return i->second.function( fb, this );
  }
}

void Dispatcher::OSC_put( const Parser::OSC_Put *act )
{
  assert( act->char_present );
  if ( OSC_string.size() < 256 ) { /* should be a long enough window title */
    OSC_string.push_back( act->ch );
  }
}

void Dispatcher::OSC_start( const Parser::OSC_Start *act __attribute((unused)) )
{
  OSC_string.clear();
}

bool Dispatcher::operator==( const Dispatcher &x ) const
{
  return ( params == x.params ) && ( parsed_params == x.parsed_params ) && ( parsed == x.parsed )
    && ( dispatch_chars == x.dispatch_chars ) && ( OSC_string == x.OSC_string ) && ( terminal_to_host == x.terminal_to_host );
}
