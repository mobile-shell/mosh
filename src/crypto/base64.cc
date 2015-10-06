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
#include <stdlib.h>

#include "fatal_assert.h"
#include "base64.h"

static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool base64_decode( const char *b64, const size_t b64_len,
		    uint8_t *raw, size_t *raw_len )
{
  fatal_assert( b64_len == 24 ); /* only useful for Mosh keys */
  fatal_assert( *raw_len == 16 );

  uint32_t bytes = 0;
  for (int i = 0; i < 22; i++) {
    const char *match = strchr(table, *(b64++)); /* Yes.  Inefficient.  It doesn't matter. */
    if (!match) {
      return false;
    }
    bytes <<= 6;
    bytes += match - table;
    /* write groups of 3 */
    if (i % 4 == 3) {
      raw[0] = bytes >> 16;
      raw[1] = bytes >> 8;
      raw[2] = bytes;
      raw += 3;
      bytes = 0;
    }
  }
  /* last byte of output */
  *(raw++) = bytes >> 4;
  if (*(b64++) != '=' || *(b64++) != '=') {
    return false;
  }
  return true;
}

void base64_encode( const uint8_t *raw, const size_t raw_len,
		    char *b64, const size_t b64_len )
{
  fatal_assert( b64_len == 24 ); /* only useful for Mosh keys */
  fatal_assert( raw_len == 16 );

  /* first 15 bytes of input */
  for (int i = 0; i < 5; i++) {
    uint32_t bytes = (raw[0] << 16) | (raw[1] << 8) | raw[2];
    b64[0] = table[(bytes >> 18) & 0x3f];
    b64[1] = table[(bytes >> 12) & 0x3f];
    b64[2] = table[(bytes >> 6) & 0x3f];
    b64[3] = table[(bytes) & 0x3f];
    raw += 3;
    b64 += 4;
  }
  
  /* last byte of input */
  *(b64++) = table[*raw >> 2];
  *(b64++) = table[(*raw << 4) & 0x3f];
  *(b64++) = '=';
  *(b64++) = '=';
}
