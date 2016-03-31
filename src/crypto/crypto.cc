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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/resource.h>
#include <fstream>

#include "byteorder.h"
#include "crypto.h"
#include "base64.h"
#include "fatal_assert.h"
#include "prng.h"

using namespace std;
using namespace Crypto;

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

uint64_t Crypto::unique( void )
{
  static uint64_t counter = 0;
  uint64_t rv = counter++;
  if ( counter == 0 ) {
    throw CryptoException( "Counter wrapped", true );
  }
  return rv;
}

AlignedBuffer::AlignedBuffer( size_t len, const char *data )
  : m_len( len ), m_allocated( NULL ), m_data( NULL )
{
  size_t alloc_len = len ? len : 1;
#if defined(HAVE_POSIX_MEMALIGN)
  if ( ( 0 != posix_memalign( &m_allocated, 16, alloc_len ) )
      || ( m_allocated == NULL ) ) {
    throw std::bad_alloc();
  }
  m_data = (char *) m_allocated;

#else
  /* malloc() a region 15 bytes larger than we need, and find
     the aligned offset within. */
  m_allocated = malloc( 15 + alloc_len );
  if ( m_allocated == NULL ) {
    throw std::bad_alloc();
  }

  uintptr_t iptr = (uintptr_t) m_allocated;
  if ( iptr & 0xF ) {
    iptr += 16 - ( iptr & 0xF );
  }
  assert( !( iptr & 0xF ) );
  assert( iptr >= (uintptr_t) m_allocated );
  assert( iptr <= ( 15 + (uintptr_t) m_allocated ) );

  m_data = (char *) iptr;

#endif /* !defined(HAVE_POSIX_MEMALIGN) */

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
  if ( !base64_decode( base64.data(), 24, key, &len ) ) {
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
  PRNG().fill( key, sizeof( key ) );
}

Base64Key::Base64Key(PRNG &prng)
{
  prng.fill( key, sizeof( key ) );
}

string Base64Key::printable_key( void ) const
{
  char base64[ 24 ];
  
  base64_encode( key, 16, base64, 24 );

  if ( (base64[ 23 ] != '=')
       || (base64[ 22 ] != '=') ) {
    throw CryptoException( string( "Unexpected output from base64_encode: " ) + string( base64, 24 ) );
  }

  base64[ 22 ] = 0;
  return string( base64 );
}

Session::Session( Base64Key s_key )
  : key( s_key ), ctx_buf( ae_ctx_sizeof() ),
    ctx( (ae_ctx *)ctx_buf.data() ), blocks_encrypted( 0 ),
    plaintext_buffer( RECEIVE_MTU ),
    ciphertext_buffer( RECEIVE_MTU ),
    nonce_buffer( Nonce::NONCE_LEN )
{
  if ( AE_SUCCESS != ae_init( ctx, key.data(), 16, 12, 16 ) ) {
    throw CryptoException( "Could not initialize AES-OCB context." );
  }
}

Session::~Session()
{
  fatal_assert( ae_clear( ctx ) == AE_SUCCESS );
}

Nonce::Nonce( uint64_t val )
{
  uint64_t val_net = htobe64( val );

  memset( bytes, 0, 4 );
  memcpy( bytes + 4, &val_net, 8 );
}

uint64_t Nonce::val( void ) const
{
  uint64_t ret;
  memcpy( &ret, bytes + 4, 8 );
  return be64toh( ret );
}

Nonce::Nonce( const char *s_bytes, size_t len )
{
  if ( len != 8 ) {
    throw CryptoException( "Nonce representation must be 8 octets long." );
  }

  memset( bytes, 0, 4 );
  memcpy( bytes + 4, s_bytes, 8 );
}

const string Session::encrypt( const Message & plaintext )
{
  const size_t pt_len = plaintext.text.size();
  const int ciphertext_len = pt_len + 16;

  assert( (size_t)ciphertext_len <= ciphertext_buffer.len() );
  assert( pt_len <= plaintext_buffer.len() );

  memcpy( plaintext_buffer.data(), plaintext.text.data(), pt_len );
  memcpy( nonce_buffer.data(), plaintext.nonce.data(), Nonce::NONCE_LEN );

  if ( ciphertext_len != ae_encrypt( ctx,                                     /* ctx */
				     nonce_buffer.data(),                     /* nonce */
				     plaintext_buffer.data(),                 /* pt */
				     pt_len,                                  /* pt_len */
				     NULL,                                    /* ad */
				     0,                                       /* ad_len */
				     ciphertext_buffer.data(),                /* ct */
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

  string text( ciphertext_buffer.data(), ciphertext_len );

  return plaintext.nonce.cc_str() + text;
}

const Message Session::decrypt( const char *str, size_t len )
{
  if ( len < 24 ) {
    throw CryptoException( "Ciphertext must contain nonce and tag." );
  }

  int body_len = len - 8;
  int pt_len = body_len - 16;

  if ( pt_len < 0 ) { /* super-assertion that pt_len does not equal AE_INVALID */
    fprintf( stderr, "BUG.\n" );
    exit( 1 );
  }

  assert( (size_t)body_len <= ciphertext_buffer.len() );
  assert( (size_t)pt_len <= plaintext_buffer.len() );

  Nonce nonce( str, 8 );
  memcpy( ciphertext_buffer.data(), str + 8, body_len );
  memcpy( nonce_buffer.data(), nonce.data(), Nonce::NONCE_LEN );

  if ( pt_len != ae_decrypt( ctx,                      /* ctx */
			     nonce_buffer.data(),      /* nonce */
			     ciphertext_buffer.data(), /* ct */
			     body_len,                 /* ct_len */
			     NULL,                     /* ad */
			     0,                        /* ad_len */
			     plaintext_buffer.data(),  /* pt */
			     NULL,                     /* tag */
			     AE_FINALIZE ) ) {         /* final */
    throw CryptoException( "Packet failed integrity check." );
  }

  const Message ret( nonce, string( plaintext_buffer.data(), pt_len ) );

  return ret;
}

static rlim_t saved_core_rlimit;

/* Disable dumping core, as a precaution to avoid saving sensitive data
   to disk. */
void Crypto::disable_dumping_core( void ) {
  struct rlimit limit;
  if ( 0 != getrlimit( RLIMIT_CORE, &limit ) ) {
    /* We don't throw CryptoException because this is called very early
       in main(), outside of 'try'. */
    perror( "getrlimit(RLIMIT_CORE)" );
    exit( 1 );
  }

  saved_core_rlimit = limit.rlim_cur;
  limit.rlim_cur = 0;
  if ( 0 != setrlimit( RLIMIT_CORE, &limit ) ) {
    perror( "setrlimit(RLIMIT_CORE)" );
    exit( 1 );
  }
}

void Crypto::reenable_dumping_core( void ) {
  /* Silent failure is safe. */
  struct rlimit limit;
  if ( 0 == getrlimit( RLIMIT_CORE, &limit ) ) {
    limit.rlim_cur = saved_core_rlimit;
    setrlimit( RLIMIT_CORE, &limit );
  }
}
