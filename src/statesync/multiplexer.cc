#include "multiplexer.h"
#include <stdio.h>

using namespace Network;

void MultiplexerStream::subtract( const Stream *prefixStream )
{
  const MultiplexerStream *prefix = dynamic_cast<const MultiplexerStream*>(prefixStream);
  streams.at(0)->subtract(prefix->streams.at(0));
}

std::string MultiplexerStream::diff_from( const Stream &existingStream ) const
{
  const MultiplexerStream &existing = dynamic_cast<const MultiplexerStream&>(existingStream);
  return streams.at(0)->diff_from(*existing.streams.at(0));
}

void MultiplexerStream::apply_string( const std::string &diff )
{
  streams.at(0)->apply_string(diff);
}

MultiplexerStream* MultiplexerStream::copy(void) const {
  return new MultiplexerStream(streams);
}
