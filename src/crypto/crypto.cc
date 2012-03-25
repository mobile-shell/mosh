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
*/

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/resource.h>

#include "byteorder.h"
#include "crypto.h"
#include "base64.h"

using namespace std;
using namespace Crypto;

const char rdev[] = "/dev/urandom";

long int myatoi( const char *str )
{
  char *end;

  errno = 0;
  long int ret = strtol( str, &end, 10 );

  if ( ( errno != 0 )
       || ( end != str + strlen( str ) ) ) {
    throw CryptoException( "Bad integer." );
  }

  return ret;
}

AlignedBuffer::AlignedBuffer( size_t len, const char *data )
  : m_len( len ), m_data( NULL )
{
  void *ptr = NULL;

#if defined(HAVE_POSIX_MEMALIGN)
  if( (0 != posix_memalign( (void **)&ptr, 16, len )) || (ptr == NULL) ) {
    throw std::bad_alloc();
  }
#else
  // Some platforms will align malloc. Let's try that first.
  if( ! (ptr = malloc(len)) ) {
    throw std::bad_alloc();
  }
  if( (uintptr_t)ptr & 0xF ) {
    // The pointer wasn't 16-byte aligned, so try again with valloc
    free(ptr);
    if( ! (ptr = valloc(len)) ) {
      throw std::bad_alloc();
    }
    if( (uintptr_t)ptr & 0xF ) {
      free(ptr);
      throw std::bad_alloc();
    }
  }
#endif /* !defined(HAVE_POSIX_MEMALIGN) */

  m_data = (char *) ptr;

  if ( data ) {
    memcpy( m_data, data, len );
  }
}

Base64Key::Base64Key( string printable_key )
{
  if ( printable_key.length() != 22 ) {
    throw CryptoException( "Key must be 22 letters long." );
  }

  string base64 = printable_key + "==";

  size_t len = 16;
  if ( !base64_decode( base64.data(), 24, (char *)&key[ 0 ], &len ) ) {
    throw CryptoException( "Key must be well-formed base64." );
  }

  if ( len != 16 ) {
    throw CryptoException( "Key must represent 16 octets." );
  }

  /* to catch changes after the first 128 bits */
  if ( printable_key != this->printable_key() ) {
    throw CryptoException( "Base64 key was not encoded 128-bit key." );
  }
}

Base64Key::Base64Key()
{
  FILE *devrandom = fopen( rdev, "r" );
  if ( devrandom == NULL ) {
    throw CryptoException( string( rdev ) + ": " + strerror( errno ) );
  }

  if ( 1 != fread( key, 16, 1, devrandom ) ) {
    throw CryptoException( "Could not read from " + string( rdev ) );
  }

  if ( 0 != fclose( devrandom ) ) {
    throw CryptoException( string( rdev ) + ": " + strerror( errno ) );
  }
}

string Base64Key::printable_key( void ) const
{
  char base64[ 25 ];
  
  base64_encode( (char *)key, 16, base64, 25 );

  if ( (base64[ 24 ] != 0)
       || (base64[ 23 ] != '=')
       || (base64[ 22 ] != '=') ) {
    throw CryptoException( "Unexpected output from base64_encode." );
  }

  base64[ 22 ] = 0;
  return string( base64 );
}

Session::Session( Base64Key s_key )
  : key( s_key ), ctx( NULL ), blocks_encrypted( 0 )
{
  ctx = ae_allocate( NULL );
  if ( ctx == NULL ) {
    throw CryptoException( "Could not allocate AES-OCB context." );
  }

  if ( AE_SUCCESS != ae_init( ctx, key.data(), 16, 12, 16 ) ) {
    throw CryptoException( "Could not initialize AES-OCB context." );
  }
}

Session::~Session()
{
  if ( ae_clear( ctx ) != AE_SUCCESS ) {
    throw CryptoException( "Could not clear AES-OCB context." );
  }

  ae_free( ctx );
}

Nonce::Nonce( uint64_t val )
{
  uint64_t val_net = htobe64( val );

  memset( bytes, 0, 4 );
  memcpy( bytes + 4, &val_net, 8 );
}

uint64_t Nonce::val( void )
{
  uint64_t ret;
  memcpy( &ret, bytes + 4, 8 );
  return be64toh( ret );
}

Nonce::Nonce( char *s_bytes, size_t len )
{
  if ( len != 8 ) {
    throw CryptoException( "Nonce representation must be 8 octets long." );
  }

  memset( bytes, 0, 4 );
  memcpy( bytes + 4, s_bytes, 8 );
}

Message::Message( char *nonce_bytes, size_t nonce_len,
		  char *text_bytes, size_t text_len )
  : nonce( nonce_bytes, nonce_len ),
    text( (char *)text_bytes, text_len )
{}

Message::Message( Nonce s_nonce, string s_text )
  : nonce( s_nonce ),
    text( s_text )
{}

string Session::encrypt( Message plaintext )
{
  const size_t pt_len = plaintext.text.size();
  const int ciphertext_len = pt_len + 16;

  AlignedBuffer ciphertext( ciphertext_len );
  AlignedBuffer pt( pt_len, plaintext.text.data() );

  if ( (uint64_t( plaintext.nonce.data() ) & 0xf) != 0 ) {
    throw CryptoException( "Bad alignment." );
  }

  if ( ciphertext_len != ae_encrypt( ctx,                                     /* ctx */
				     plaintext.nonce.data(),                  /* nonce */
				     pt.data(),                               /* pt */
				     pt.len(),                                /* pt_len */
				     NULL,                                    /* ad */
				     0,                                       /* ad_len */
				     ciphertext.data(),                       /* ct */
				     NULL,                                    /* tag */
				     AE_FINALIZE ) ) {                        /* final */
    throw CryptoException( "ae_encrypt() returned error." );
  }

  blocks_encrypted += pt_len >> 4;
  if ( pt_len & 0xF ) {
    /* partial block */
    blocks_encrypted++;
  }

  /* "Both the privacy and the authenticity properties of OCB degrade as
      per s^2 / 2^128, where s is the total number of blocks that the
      adversary acquires.... In order to ensure that s^2 / 2^128 remains
      small, a given key should be used to encrypt at most 2^48 blocks (2^55
      bits or 4 petabytes)"

     -- http://tools.ietf.org/html/draft-krovetz-ocb-03

     We deem it unlikely that a legitimate user will send 4 PB through a Mosh
     session.  If it happens, we simply kill the session.  The server and
     client use the same key, so we actually need to die after 2^47 blocks.
  */
  if ( blocks_encrypted >> 47 ) {
    throw CryptoException( "Encrypted 2^47 blocks.", true );
  }

  string text( ciphertext.data(), ciphertext.len() );

  return plaintext.nonce.cc_str() + text;
}

Message Session::decrypt( string ciphertext )
{
  if ( ciphertext.size() < 24 ) {
    throw CryptoException( "Ciphertext must contain nonce and tag." );
  }

  char *str = (char *)ciphertext.data();

  int body_len = ciphertext.size() - 8;
  int pt_len = body_len - 16;

  if ( pt_len < 0 ) { /* super-assertion that pt_len does not equal AE_INVALID */
    fprintf( stderr, "BUG.\n" );
    exit( 1 );
  }

  Nonce __attribute__((__aligned__ (16))) nonce( str, 8 );
  AlignedBuffer body( body_len, str + 8 );
  AlignedBuffer plaintext( pt_len );

  if ( pt_len != ae_decrypt( ctx,               /* ctx */
			     nonce.data(),      /* nonce */
			     body.data(),       /* ct */
			     body.len(),        /* ct_len */
			     NULL,              /* ad */
			     0,                 /* ad_len */
			     plaintext.data(),  /* pt */
			     NULL,              /* tag */
			     AE_FINALIZE ) ) {  /* final */
    throw CryptoException( "Packet failed integrity check." );
  }

  Message ret( nonce, string( plaintext.data(), plaintext.len() ) );

  return ret;
}

/* Disable dumping core, as a precaution to avoid saving sensitive data
   to disk. */
void Crypto::disable_dumping_core( void ) {
  struct rlimit limit;
  limit.rlim_cur = 0;
  limit.rlim_max = 0;
  if ( 0 != setrlimit( RLIMIT_CORE, &limit ) ) {
    /* We don't throw CryptoException because this is called very early
       in main(), outside of 'try'. */
    perror( "setrlimit(RLIMIT_CORE)" );
    exit( 1 );
  }
}
