#include "multiplexer.h"

using namespace Network;

void MultiplexerStream::subtract( const Stream *prefixStream )
{
  streams.at(0)->subtract(prefixStream);
}

std::string MultiplexerStream::diff_from( const Stream &existingStream ) const
{
  return streams.at(0)->diff_from(existingStream);
}

void MultiplexerStream::apply_string( const std::string &diff )
{
  streams.at(0)->apply_string(diff);
}
