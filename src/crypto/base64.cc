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
#include <openssl/bio.h>
#include <openssl/evp.h>

#include "fatal_assert.h"

bool base64_decode( const char *b64, const size_t b64_len,
		    char *raw, size_t *raw_len )
{
  bool ret = false;

  fatal_assert( b64_len == 24 ); /* only useful for Mosh keys */
  fatal_assert( *raw_len == 16 );

  /* initialize input/output */
  BIO_METHOD *b64_method = BIO_f_base64();
  fatal_assert( b64_method );

  BIO *b64_bio = BIO_new( b64_method );
  fatal_assert( b64_bio );

  BIO_set_flags( b64_bio, BIO_FLAGS_BASE64_NO_NL );

  BIO *mem_bio = BIO_new_mem_buf( (void *) b64, b64_len );
  fatal_assert( mem_bio );

  BIO *combined_bio = BIO_push( b64_bio, mem_bio );
  fatal_assert( combined_bio );

  fatal_assert( 1 == BIO_flush( combined_bio ) );
  
  /* read the string */
  int bytes_read = BIO_read( combined_bio, raw, *raw_len );
  if ( bytes_read <= 0 ) {
    goto end;
  }

  if ( bytes_read != (int)*raw_len ) {
    goto end;
  }

  fatal_assert( 1 == BIO_flush( combined_bio ) );

  /* check if there is more to read */
  char extra[ 256 ];
  bytes_read = BIO_read( combined_bio, extra, 256 );
  if ( bytes_read > 0 ) {
    goto end;
  }

  /* check if mem buf is empty */
  if ( !BIO_eof( mem_bio ) ) {
    goto end;
  }

  ret = true;
 end:
  BIO_free_all( combined_bio );
  return ret;
}

void base64_encode( const char *raw, const size_t raw_len,
		    char *b64, const size_t b64_len )
{
  fatal_assert( b64_len == 24 ); /* only useful for Mosh keys */
  fatal_assert( raw_len == 16 );

  /* initialize input/output */
  BIO_METHOD *b64_method = BIO_f_base64(), *mem_method = BIO_s_mem();
  fatal_assert( b64_method );
  fatal_assert( mem_method );

  BIO *b64_bio = BIO_new( b64_method ), *mem_bio = BIO_new( mem_method );
  fatal_assert( b64_bio );
  fatal_assert( mem_bio );

  BIO_set_flags( b64_bio, BIO_FLAGS_BASE64_NO_NL );

  BIO *combined_bio = BIO_push( b64_bio, mem_bio );
  fatal_assert( combined_bio );
  
  /* write the string */
  int bytes_written = BIO_write( combined_bio, raw, raw_len  );
  fatal_assert( bytes_written >= 0 );

  fatal_assert( bytes_written == (int)raw_len );

  fatal_assert( 1 == BIO_flush( combined_bio ) );

  /* check if mem buf has desired length */
  fatal_assert( BIO_pending( mem_bio ) == (int)b64_len );

  char *mem_ptr;

  BIO_get_mem_data( mem_bio, &mem_ptr );

  memcpy( b64, mem_ptr, b64_len );

  BIO_free_all( combined_bio );
}
