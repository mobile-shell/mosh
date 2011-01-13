#include <assert.h>
#include <typeinfo>

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
