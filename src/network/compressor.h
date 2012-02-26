#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <string>

namespace Network {
  class Compressor {
  private:
    static const int BUFFER_SIZE = 2048 * 2048; /* effective limit on terminal size */

    unsigned char *buffer;

  public:
    Compressor() : buffer( NULL ) { buffer = new unsigned char[ BUFFER_SIZE ]; }
    ~Compressor() { if ( buffer ) { delete[] buffer; } }

    std::string compress_str( const std::string input );
    std::string uncompress_str( const std::string input );

    /* unused */
    Compressor( const Compressor & );
    Compressor & operator=( const Compressor & );
  };

  Compressor & get_compressor( void );
}

#endif
