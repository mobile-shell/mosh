#include <zlib.h>
#include <assert.h>

#include "compressor.h"

using namespace Network;
using namespace std;

string Compressor::compress_str( const string input )
{
  long unsigned int len = BUFFER_SIZE;
  assert( Z_OK == compress( buffer, &len,
			    reinterpret_cast<const unsigned char *>( input.data() ),
			    input.size() ) );
  return string( reinterpret_cast<char *>( buffer ), len );
}

string Compressor::uncompress_str( const string input )
{
  long unsigned int len = BUFFER_SIZE;
  assert( Z_OK == uncompress( buffer, &len,
			      reinterpret_cast<const unsigned char *>( input.data() ),
			      input.size() ) );
  return string( reinterpret_cast<char *>( buffer ), len );
}

/* construct on first use */
Compressor & Network::get_compressor( void )
{
  static Compressor the_compressor;
  return the_compressor;
}
