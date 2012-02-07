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

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "swrite.h"

int swrite( int fd, const char *str, ssize_t len )
{
  ssize_t total_bytes_written = 0;
  ssize_t bytes_to_write = ( len >= 0 ) ? len : strlen( str );
  while ( total_bytes_written < bytes_to_write ) {
    ssize_t bytes_written = write( fd, str + total_bytes_written,
				   bytes_to_write - total_bytes_written );
    if ( bytes_written <= 0 ) {
      perror( "write" );
      return -1;
    } else {
      total_bytes_written += bytes_written;
    }
  }

  return 0;
}
