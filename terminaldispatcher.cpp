#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "terminaldispatcher.hpp"
#include "parseraction.hpp"

using namespace Terminal;

Dispatcher::Dispatcher()
  : params(), parsed_params(), parsed( false ), dispatch_chars()
{}

void Dispatcher::newparamchar( Parser::Param *act )
{
  assert( act->char_present );
  assert( (act->ch == ';') || ( (act->ch >= '0') && (act->ch <= '9') ) );
  if ( params.length() < 100 ) {
    /* enough for 16 five-char params plus 15 semicolons */
    params.push_back( act->ch );
    act->handled = true;
  }
  parsed = false;
}

void Dispatcher::collect( Parser::Collect *act )
{
  assert( act->char_present );
  if ( ( dispatch_chars.length() < 8 ) /* never should need more than 2 */
       && ( act->ch <= 255 ) ) {  /* ignore non-8-bit */    
    dispatch_chars.push_back( act->ch );
    act->handled = true;
  }
}

void Dispatcher::clear( Parser::Clear *act )
{
  params.clear();
  dispatch_chars.clear();
  parsed = false;
  act->handled = true;
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
    int val = strtol( segment_begin, &endptr, 10 );
    if ( endptr == segment_begin ) {
      val = -1;
    }
    if ( errno == 0 ) {
      parsed_params.push_back( val );
    }

    segment_begin = segment_end + 1;
  }

  /* get last param */
  errno = 0;
  char *endptr;
  int val = strtol( segment_begin, &endptr, 10 );
  if ( endptr == segment_begin ) {
    val = -1;
  }
  if ( errno == 0 ) {
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

std::string Dispatcher::str( void )
{
  char assum[ 64 ];
  snprintf( assum, 64, "[dispatch=\"%s\" params=\"%s\"]",
	    dispatch_chars.c_str(), params.c_str() );
  return std::string( assum );
}
