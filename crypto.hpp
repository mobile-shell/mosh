#ifndef CRYPTO_HPP
#define CRYPTO_HPP

#include "ae.hpp"
#include <string>
#include <string.h>

using namespace std;

long int myatoi( char *str );

namespace Crypto {
  class CryptoException {
  public:
    string text;
    CryptoException( string s_text ) : text( s_text ) {};
  };

  class Base64Key {
  private:
    unsigned char key[ 16 ];

  public:
    Base64Key(); /* random key */
    Base64Key( string printable_key );
    string printable_key( void ) const;
    unsigned char *data( void ) { return key; }
  };

  class Nonce {
  private:
    char bytes[ 12 ];

  public:
    Nonce( uint64_t val );
    Nonce( char *s_bytes, size_t len );
    
    string cpp_str( void ) { return string( (char *)( bytes + 4 ), 8 ); }
    char *data( void ) { return bytes; }
    uint64_t val( void );
  };
  
  class Message {
  public:
    Nonce nonce;
    string text;
    
    Message( char *nonce_bytes, size_t nonce_len,
	     char *text_bytes, size_t text_len );
    Message( Nonce s_nonce, string s_text );
  };
  
  class Session {
  private:
    Base64Key key;
    ae_ctx *ctx;
    
  public:
    Session( Base64Key s_key );
    ~Session();
    
    string encrypt( Message plaintext );
    Message decrypt( string ciphertext );
    
    Session( const Session & );
    Session & operator=( const Session & );
  };
}

#endif
