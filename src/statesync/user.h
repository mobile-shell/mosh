#ifndef USER_HPP
#define USER_HPP

#include <deque>
#include <list>
#include <string>
#include <assert.h>

#include "parseraction.h"

using namespace std;

namespace Network {
  enum UserEventType {
    UserByteType = 0,
    ResizeType = 1
  };

  class UserEvent
  {
  public:
    UserEventType type;
    Parser::UserByte userbyte;
    Parser::Resize resize;

    UserEvent( Parser::UserByte s_userbyte ) : type( UserByteType ), userbyte( s_userbyte ), resize( -1, -1 ) {}
    UserEvent( Parser::Resize s_resize ) : type( ResizeType ), userbyte( 0 ), resize( s_resize ) {}

    UserEvent() /* default constructor required by C++11 STL */
      : type( UserByteType ),
	userbyte( 0 ),
	resize( -1, -1 )
    {
      assert( false );
    }

    bool operator==( const UserEvent &x ) const { return ( type == x.type ) && ( userbyte == x.userbyte ) && ( resize == x.resize ); }
  };

  class UserStream
  {
  private:
    deque<UserEvent> actions;
    
  public:
    UserStream() : actions() {}
    
    void push_back( Parser::UserByte s_userbyte ) { actions.push_back( UserEvent( s_userbyte ) ); }
    void push_back( Parser::Resize s_resize ) { actions.push_back( UserEvent( s_resize ) ); }
    
    size_t size( void ) { return actions.size(); }
    const Parser::Action *get_action( unsigned int i );
    
    /* interface for Network::Transport */
    void subtract( const UserStream *prefix );
    string diff_from( const UserStream &existing );
    void apply_string( string diff );
    bool operator==( const UserStream &x ) const { return actions == x.actions; }
  };
}

#endif
