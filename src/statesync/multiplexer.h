#ifndef MULTIPLEXER_HPP
#define MULTIPLEXER_HPP

#include <vector>
#include <string>

#include "stream.h"

namespace Network {
  class MultiplexerStream : public Stream {
  private:
    std::vector<Stream*> streams;

  public:
    MultiplexerStream(std::vector<Stream*> s) : streams(s) {}

    template <typename T>
    T* stream(int i) const {
      return dynamic_cast<T*>(streams.at(i));
    }

    /* interface for Network::Transport */
    void subtract( const Stream *prefix );
    std::string diff_from( const Stream &existing ) const;
    std::string init_diff( void ) const {
      std::vector<Stream*> empty;
      return diff_from( MultiplexerStream(empty) );
    };
    void apply_string( const std::string &diff );
    bool operator==( const Stream &xStream ) const {
      const MultiplexerStream &x = dynamic_cast<const MultiplexerStream&>(xStream);
      return streams == x.streams;
    }

    bool compare( const MultiplexerStream & ) { return false; }
  };
}

#endif
