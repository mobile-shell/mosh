#include "parser.hpp"

std::vector<Parser::Action *> Parser::Parser::input( wchar_t ch )
{
  std::vector<Action *> ret;

  Transition tx = state->input( ch );

  if ( tx.next_state != NULL ) {
    Action *exitact = state->exit();
    if ( exitact ) {
      ret.push_back( exitact );
    }
  }

  if ( tx.action ) {
    ret.push_back( tx.action );
  }

  if ( tx.next_state != NULL ) {
    Action *enteract = tx.next_state->enter();
    if ( enteract ) {
      ret.push_back( enteract );
    }
    state = tx.next_state;
  }

  return ret;
}
