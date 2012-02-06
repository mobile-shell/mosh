#include <assert.h>
#include <typeinfo>

#include "user.h"
#include "userinput.pb.h"

using namespace Parser;
using namespace Network;
using namespace ClientBuffers;

void UserStream::subtract( const UserStream *prefix )
{
  for ( deque<UserEvent>::const_iterator i = prefix->actions.begin();
	i != prefix->actions.end();
	i++ ) {
    assert( !actions.empty() );
    assert( *i == actions.front() );
    actions.pop_front();
  }
}

string UserStream::diff_from( const UserStream &existing )
{
  deque<UserEvent>::iterator my_it = actions.begin();

  for ( deque<UserEvent>::const_iterator i = existing.actions.begin();
	i != existing.actions.end();
	i++ ) {
    assert( my_it != actions.end() );
    assert( *i == *my_it );
    my_it++;
  }

  ClientBuffers::UserMessage output;

  while ( my_it != actions.end() ) {
    switch ( my_it->type ) {
    case UserByteType:
      {
	char the_byte = my_it->userbyte.c;
	/* can we combine this with a previous Keystroke? */
	if ( (output.instruction_size() > 0)
	     && (output.instruction( output.instruction_size() - 1 ).HasExtension( keystroke )) ) {
	  output.mutable_instruction( output.instruction_size() - 1 )->MutableExtension( keystroke )->mutable_keys()->append( string( &the_byte, 1 ) );
	} else {
	  Instruction *new_inst = output.add_instruction();
	  new_inst->MutableExtension( keystroke )->set_keys( &the_byte, 1 );
	}
      }
      break;
    case ResizeType:
      {
	Instruction *new_inst = output.add_instruction();
	new_inst->MutableExtension( resize )->set_width( my_it->resize.width );
	new_inst->MutableExtension( resize )->set_height( my_it->resize.height );
      }
      break;
    default:
      assert( false );
    }

    my_it++;
  }

  return output.SerializeAsString();
}

void UserStream::apply_string( string diff )
{
  ClientBuffers::UserMessage input;
  assert( input.ParseFromString( diff ) );

  for ( int i = 0; i < input.instruction_size(); i++ ) {
    if ( input.instruction( i ).HasExtension( keystroke ) ) {
      string the_bytes = input.instruction( i ).GetExtension( keystroke ).keys();
      for ( unsigned int loc = 0; loc < the_bytes.size(); loc++ ) {
	actions.push_back( UserEvent( UserByte( the_bytes.at( loc ) ) ) );
      }
    } else if ( input.instruction( i ).HasExtension( resize ) ) {
      actions.push_back( UserEvent( Resize( input.instruction( i ).GetExtension( resize ).width(),
					    input.instruction( i ).GetExtension( resize ).height() ) ) );
    }
  }
}

const Parser::Action *UserStream::get_action( unsigned int i )
{
  switch( actions[ i ].type ) {
  case UserByteType:
    return &( actions[ i ].userbyte );
  case ResizeType:
    return &( actions[ i ].resize );
  default:
    assert( false );
    return NULL;
  }
}
