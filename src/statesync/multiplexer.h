#ifndef MULTIPLEXER_HPP
#define MULTIPLEXER_HPP

#include <vector>
#include <string>
#include <assert.h>

#include "stream.h"
#include "stream.pb.h"
#include "fatal_assert.h"

namespace Network {
  class MultiplexerStream : public Stream {
  private:
    std::vector<Stream*> streams;

  public:
    MultiplexerStream(std::vector<Stream*> s) : streams() {
      for (Stream *s : s) {
        streams.push_back(s->copy());
      }
    }
    MultiplexerStream(const MultiplexerStream & other) : streams() {
      for (Stream *s : other.streams) {
        streams.push_back(s->copy());
      }
    }

    ~MultiplexerStream() {
      for (Stream *s : streams) {
        delete s;
      }
      streams.clear();
    }

    template <typename T>
    T* stream(int i) const {
      Stream* stream = streams.at(i);
      assert(stream != NULL);
      return dynamic_cast<T*>(stream);
    }

    void set(int i, Stream *s) {
      Stream *old = streams.at(i);
      delete old;
      streams.at(i) = s->copy();
    }

    /* interface for Network::Transport */
    void subtract( const Stream *prefix );
    std::string diff_from( const Stream &existing ) const;
    std::string init_diff( void ) const;
    void apply_string( const std::string &diff );
    bool operator==( const Stream &xStream ) const {
      const MultiplexerStream &x = dynamic_cast<const MultiplexerStream&>(xStream);
      return streams == x.streams;
    }

    bool compare( const MultiplexerStream & ) { return false; }
    MultiplexerStream* copy(void) const;

    static std::string diffForStream(int i, std::string diff) {
      StreamBuffers::StreamMessage input;
      fatal_assert( input.ParseFromString( diff ) );
      return input.diffs(i);
    };
  };
}

#endif
