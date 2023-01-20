#include <stdio.h>
#include <assert.h>

#include "stream.pb.h"
#include "fatal_assert.h"
#include "multiplexer.h"

using namespace Network;

void MultiplexerStream::subtract( const Stream *prefixStream )
{
  const MultiplexerStream *prefix = dynamic_cast<const MultiplexerStream*>(prefixStream);
  assert(streams.size() == prefix->streams.size());
  for (std::vector<Stream *>::size_type i = 0; i < prefix->streams.size(); i++) {
    streams.at(i)->subtract(prefix->streams.at(i));
  }
}

std::string MultiplexerStream::diff_from( const Stream &existingStream ) const
{
  const MultiplexerStream &existing = dynamic_cast<const MultiplexerStream&>(existingStream);
  assert(streams.size() == existing.streams.size());
  StreamBuffers::StreamMessage output;
  for (std::vector<Stream *>::size_type i = 0; i < existing.streams.size(); i++) {
    std::string diff = streams.at(i)->diff_from(*existing.streams.at(i));
    output.add_diffs(diff);
  }
  return output.SerializeAsString();
}

void MultiplexerStream::apply_string( const std::string &diff )
{
  StreamBuffers::StreamMessage input;
  fatal_assert( input.ParseFromString( diff ) );
  assert(static_cast<int>(streams.size()) == input.diffs_size());
  for (int i = 0; i < input.diffs_size(); i++) {
    streams.at(i)->apply_string(input.diffs(i));
  }
}

MultiplexerStream* MultiplexerStream::copy(void) const {
  return new MultiplexerStream(streams);
}

std::string MultiplexerStream::init_diff( void ) const {
  StreamBuffers::StreamMessage output;
  for (std::vector<Stream *>::size_type i = 0; i < streams.size(); i++) {
    std::string diff = streams.at(i)->init_diff();
    output.add_diffs(diff);
  }
  return output.SerializeAsString();
}
