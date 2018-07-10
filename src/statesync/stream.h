#ifndef STREAM_HPP
#define STREAM_HPP

#include <string>

namespace Network {
  class Stream {
  public:
    /* interface for Network::Transport */
    virtual void subtract( const Stream *prefix ) = 0;
    virtual std::string diff_from( const Stream &existing ) const = 0;
    virtual std::string init_diff( void ) const = 0;
    virtual void apply_string( const std::string &diff ) = 0;
    virtual bool operator==( const Stream &x ) const = 0;
  };
}

#endif
