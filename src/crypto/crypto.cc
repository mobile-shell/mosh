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
#include <sys/errno.h>
#include <sys/endian.h>

#include "crypto.h"
#include "base64.h"

using namespace std;
using namespace Crypto;

const char rdev[] = "/dev/urandom";

long int myatoi( char *str )
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

static void * sse_alloc( int len )
{
  void *ptr = NULL;

  if( (0 != posix_memalign( (void **)&ptr, 16, len )) || (ptr == NULL) ) {
    throw std::bad_alloc();
  }

  return ptr;
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
  : key( s_key ), ctx( NULL )
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

  char *ciphertext = (char *)sse_alloc( ciphertext_len );
  char *pt = (char *)sse_alloc( pt_len );

  memcpy( pt, plaintext.text.data(), plaintext.text.size() );

  if ( (uint64_t( plaintext.nonce.data() ) & 0xf) != 0 ) {
    throw CryptoException( "Bad alignment." );
  }

  if ( ciphertext_len != ae_encrypt( ctx,                                     /* ctx */
				     plaintext.nonce.data(),                  /* nonce */
				     pt,                                      /* pt */
				     pt_len,                                  /* pt_len */
				     NULL,                                    /* ad */
				     0,                                       /* ad_len */
				     ciphertext,                              /* ct */
				     NULL,                                    /* tag */
				     AE_FINALIZE ) ) {                        /* final */
    free( pt );
    free( ciphertext );
    throw CryptoException( "ae_encrypt() returned error." );
  }

  string text( (char *)ciphertext, ciphertext_len );
  free( pt );
  free( ciphertext );

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
  char *body = (char *)sse_alloc( body_len );
  memcpy( body, str + 8, body_len );

  char *plaintext = (char *)sse_alloc( pt_len );

  if ( pt_len != ae_decrypt( ctx,               /* ctx */
			     nonce.data(),      /* nonce */
			     body,              /* ct */
			     body_len,          /* ct_len */
			     NULL,              /* ad */
			     0,                 /* ad_len */
			     plaintext,         /* pt */
			     NULL,              /* tag */
			     AE_FINALIZE ) ) {  /* final */
    free( plaintext );
    free( body );
    throw CryptoException( "Packet failed integrity check." );
  }

  Message ret( nonce, string( plaintext, pt_len ) );
  free( plaintext );
  free( body );

  return ret;
}
