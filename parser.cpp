#include <assert.h>
#include <typeinfo>
#include <langinfo.h>

#include "parser.hpp"

static void append_or_delete( Parser::Action *act,
			      std::vector<Parser::Action *>&vec )
{
  assert( act );

  if ( typeid( *act ) != typeid( Parser::Ignore ) ) {
    vec.push_back( act );
  } else {
    delete act;
  }
}

std::vector<Parser::Action *> Parser::Parser::input( wchar_t ch )
{
  std::vector<Action *> ret;

  Transition tx = state->input( ch );

  if ( tx.next_state != NULL ) {
    append_or_delete( state->exit(), ret );
  }

  append_or_delete( tx.action, ret );

  if ( tx.next_state != NULL ) {
    append_or_delete( tx.next_state->enter(), ret );
    state = tx.next_state;
  }

  return ret;
}

Parser::UTF8Parser::UTF8Parser()
  : parser(), buf_len( 0 )
{
  if ( strcmp( nl_langinfo( CODESET ), "UTF-8" ) != 0 ) {
    fprintf( stderr, "rtm requires a UTF-8 locale.\n" );
    throw std::string( "rtm requires a UTF-8 locale." );
  }
}

std::vector<Parser::Action *> Parser::UTF8Parser::input( char c )
{
  assert( buf_len < BUF_SIZE );

  buf[ buf_len++ ] = c;

  /* This function will only work in a UTF-8 locale. */
  /* This must be asserted by other code. */

  wchar_t pwc;
  mbstate_t ps;
  memset( &ps, 0, sizeof( ps ) );

  size_t bytes_parsed = mbrtowc( &pwc, buf, buf_len, &ps );

  /* this returns 0 when n = 0! */

  /* This function annoying returns a size_t so we have to check
     the negative values first before the "> 0" branch */

  if ( bytes_parsed == 0 ) {
    /* character was NUL, accept and clear buffer */
    assert( buf_len == 1 );
    buf_len = 0;
    pwc = L'\0';
  } else if ( bytes_parsed == (size_t) -1 ) {
    /* invalid sequence, use replacement character and clear buffer */
    assert( errno == EILSEQ );
    buf_len = 0;
    pwc = (wchar_t) 0xFFFD;
  } else if ( bytes_parsed == (size_t) -2 ) {
    /* can't parse complete multibyte character */
    /* return empty vector */
    std::vector<Action *> vec;
    return vec;
  } else if ( bytes_parsed > 0 ) {
    /* parsed into pwc, accept and clear buffer */
    assert( bytes_parsed == buf_len );
    buf_len = 0;
  } else {
    throw std::string( "Unknown return value from mbrtowc" );
  }

  /* we parsed character into pwc */
  return parser.input( pwc );
}
