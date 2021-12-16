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

#ifndef BYTEORDER_HPP
#define BYTEORDER_HPP

#include "config.h"

#if HAVE_DECL_BE64TOH || HAVE_DECL_BETOH64

# if defined(HAVE_ENDIAN_H)
#  include <endian.h>
# elif defined(HAVE_SYS_ENDIAN_H)
#  include <sys/types.h>
#  include <sys/endian.h>
# endif

#if !HAVE_DECL_BE64TOH && HAVE_DECL_BETOH64
#define be64toh betoh64
#define be16toh betoh16
#endif

#elif HAVE_OSX_SWAP
# include <libkern/OSByteOrder.h>
# define htobe64 OSSwapHostToBigInt64
# define be64toh OSSwapBigToHostInt64
# define htobe16 OSSwapHostToBigInt16
# define be16toh OSSwapBigToHostInt16

#else

/* Use our fallback implementation, which is correct for any endianness. */

#include <stdint.h>

/* Make sure they aren't macros */
#undef htobe64
#undef be64toh
#undef htobe16
#undef be16toh

/* Use unions rather than casts, to comply with strict aliasing rules. */

inline uint64_t htobe64( uint64_t x ) {
  uint8_t xs[ 8 ] = {
    static_cast<uint8_t>( ( x >> 56 ) & 0xFF ),
    static_cast<uint8_t>( ( x >> 48 ) & 0xFF ),
    static_cast<uint8_t>( ( x >> 40 ) & 0xFF ),
    static_cast<uint8_t>( ( x >> 32 ) & 0xFF ),
    static_cast<uint8_t>( ( x >> 24 ) & 0xFF ),
    static_cast<uint8_t>( ( x >> 16 ) & 0xFF ),
    static_cast<uint8_t>( ( x >>  8 ) & 0xFF ),
    static_cast<uint8_t>( ( x       ) & 0xFF ) };
  union {
    const uint8_t  *p8;
    const uint64_t *p64;
  } u;
  u.p8 = xs;
  return *u.p64;
}

inline uint64_t be64toh( uint64_t x ) {
  union {
    const uint8_t  *p8;
    const uint64_t *p64;
  } u;
  u.p64 = &x;
  return ( uint64_t( u.p8[ 0 ] ) << 56 )
       | ( uint64_t( u.p8[ 1 ] ) << 48 )
       | ( uint64_t( u.p8[ 2 ] ) << 40 )
       | ( uint64_t( u.p8[ 3 ] ) << 32 )
       | ( uint64_t( u.p8[ 4 ] ) << 24 )
       | ( uint64_t( u.p8[ 5 ] ) << 16 )
       | ( uint64_t( u.p8[ 6 ] ) <<  8 )
       | ( uint64_t( u.p8[ 7 ] ) );
}

inline uint16_t htobe16( uint16_t x ) {
  uint8_t xs[ 2 ] = {
    static_cast<uint8_t>( ( x >> 8 ) & 0xFF ),
    static_cast<uint8_t>( ( x      ) & 0xFF ) };
  union {
    const uint8_t  *p8;
    const uint16_t *p16;
  } u;
  u.p8 = xs;
  return *u.p16;
}

inline uint16_t be16toh( uint16_t x ) {
  union {
    const uint8_t  *p8;
    const uint16_t *p16;
  } u;
  u.p16 = &x;
  return ( uint16_t( u.p8[ 0 ] ) << 8 )
       | ( uint16_t( u.p8[ 1 ] ) );
}

#endif

#endif
