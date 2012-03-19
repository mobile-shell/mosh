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

#ifndef PRNG_HPP
#define PRNG_HPP

#include <string>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include "crypto.h"

/* Read random bytes from /dev/urandom.

   We rely on stdio buffering for efficiency. */

static const char rdev[] = "/dev/urandom";

using namespace Crypto;

class PRNG {
 private:
  FILE *randfile;

  /* unimplemented to satisfy -Weffc++ */
  PRNG( const PRNG & );
  PRNG & operator=( const PRNG & );

 public:
  PRNG() : randfile( fopen( rdev, "rb" ) )
  {
    if ( randfile == NULL ) {
      throw CryptoException( std::string( rdev ) + ": " + strerror( errno ) );
    }
  }

  ~PRNG() {
    if ( 0 != fclose( randfile ) ) {
      throw CryptoException( std::string( rdev ) + ": " + strerror( errno ) );
    }
  }

  void fill( void *dest, size_t size ) {
    if ( 0 == size ) {
      return;
    }

    if ( 1 != fread( dest, size, 1, randfile ) ) {
      throw CryptoException( "Could not read from " + std::string( rdev ) );
    }
  }

  uint8_t uint8() {
    uint8_t x;
    fill( &x, 1 );
    return x;
  }
};

#endif
