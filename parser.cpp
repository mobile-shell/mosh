#include "parser.hpp"

std::vector<Parser::Action> Parser::Parser::input( wchar_t ch )
{
  std::vector<Action> ret;

  Transition tx = state->input( ch );

  if ( tx.next_state != NULL ) {
    ret.push_back( state->exit() );
  }

  ret.push_back( tx.action );

  if ( tx.next_state != NULL ) {
    ret.push_back( tx.next_state->enter() );
    state = tx.next_state;
  }

  return ret;
}
