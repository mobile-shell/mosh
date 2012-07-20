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

#include <zlib.h>

#include "compressor.h"
#include "dos_assert.h"

using namespace Network;
using namespace std;

string Compressor::compress_str( const string &input )
{
  long unsigned int len = BUFFER_SIZE;
  dos_assert( Z_OK == compress( buffer, &len,
				reinterpret_cast<const unsigned char *>( input.data() ),
				input.size() ) );
  return string( reinterpret_cast<char *>( buffer ), len );
}

string Compressor::uncompress_str( const string &input )
{
  long unsigned int len = BUFFER_SIZE;
  dos_assert( Z_OK == uncompress( buffer, &len,
				  reinterpret_cast<const unsigned char *>( input.data() ),
				  input.size() ) );
  return string( reinterpret_cast<char *>( buffer ), len );
}

/* construct on first use */
Compressor & Network::get_compressor( void )
{
  static Compressor the_compressor;
  return the_compressor;
}
