/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#ifndef CRYPTO_HPP
#define CRYPTO_HPP

#include "ae.h"
#include <string>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <exception>

using std::string;

long int myatoi( const char *str );

class PRNG;

namespace Crypto {

  class CryptoException : public std::exception {
  public:
    string text;
    bool fatal;
    CryptoException( string s_text, bool s_fatal = false )
      : text( s_text ), fatal( s_fatal ) {};
    const char *what() const throw () { return text.c_str(); }
    ~CryptoException() throw () {}
  };

  /*
   * OCB (and other algorithms) require a source of nonce/sequence
   * numbers that never repeats its output.  Enforce that with this
   * function.
   */
  uint64_t unique( void );

  /* 16-byte-aligned buffer, with length. */
  class AlignedBuffer {
  private:
    size_t m_len;
    void *m_allocated;
    char *m_data;

  public:
    AlignedBuffer( size_t len, const char *data = NULL );

    ~AlignedBuffer() {
      free( m_allocated );
    }

    char * data( void ) const { return m_data; }
    size_t len( void )  const { return m_len;  }

  private:
    /* Not implemented */
    AlignedBuffer( const AlignedBuffer & );
    AlignedBuffer & operator=( const AlignedBuffer & );
  };

  class Base64Key {
  private:
    unsigned char key[ 16 ];

  public:
    Base64Key(); /* random key */
    Base64Key(PRNG &prng);
    Base64Key( string printable_key );
    string printable_key( void ) const;
    unsigned char *data( void ) { return key; }
  };

  class Nonce {
  public:
    static const int NONCE_LEN = 12;

  private:
    char bytes[ NONCE_LEN ];

  public:
    Nonce( uint64_t val );
    Nonce( const char *s_bytes, size_t len );
    
    string cc_str( void ) const { return string( bytes + 4, 8 ); }
    const char *data( void ) const { return bytes; }
    uint64_t val( void ) const;
  };
  
  class Message {
  public:
    const Nonce nonce;
    const string text;
    
    Message( const char *nonce_bytes, size_t nonce_len,
	     const char *text_bytes, size_t text_len )
      : nonce( nonce_bytes, nonce_len ),
      text( text_bytes, text_len ) {}

    Message( const Nonce & s_nonce, const string & s_text )
      : nonce( s_nonce ),
      text( s_text ) {}
  };
  
  class Session {
  private:
    Base64Key key;
    AlignedBuffer ctx_buf;
    ae_ctx *ctx;
    uint64_t blocks_encrypted;

    AlignedBuffer plaintext_buffer;
    AlignedBuffer ciphertext_buffer;
    AlignedBuffer nonce_buffer;
    
  public:
    static const int RECEIVE_MTU = 2048;
    /* Overhead (not counting the nonce, which is handled by network transport) */
    static const int ADDED_BYTES = 16 /* final OCB block */;

    Session( Base64Key s_key );
    ~Session();
    
    const string encrypt( const Message & plaintext );
    const Message decrypt( const char *str, size_t len );
    const Message decrypt( const string & ciphertext ) {
      return decrypt( ciphertext.data(), ciphertext.size() );
    }
    
    Session( const Session & );
    Session & operator=( const Session & );
  };

  void disable_dumping_core( void );
  void reenable_dumping_core( void );
}

#endif
