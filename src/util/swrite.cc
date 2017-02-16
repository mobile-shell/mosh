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

#include "config.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "timestamp.h"
#include "swrite.h"
#include "fatal_assert.h"

int swrite( int fd, const char *str, ssize_t len )
{
  ssize_t total_bytes_written = 0;
  ssize_t bytes_to_write = ( len >= 0 ) ? len : (ssize_t) strlen( str );
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


int swrite_timeout( int fd, uint64_t timeout_ms, const char *str, ssize_t len )
{
  size_t total_bytes_written = 0;
  size_t bytes_to_write = ( len >= 0 ) ? len : (ssize_t) strlen( str );
  uint64_t t0 = frozen_timestamp();
  uint64_t iteration = 0;

  while ( total_bytes_written < bytes_to_write ) {
    iteration++;
    ssize_t rv = write( fd, str + total_bytes_written, bytes_to_write - total_bytes_written);
    if ( rv > 0 ) {
      total_bytes_written += rv;
      continue;
    } else if ( rv < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) {
      uint64_t t1 = frozen_timestamp();
      fatal_assert( t1 >= t0 );
      uint64_t total_time_spent = t1 - t0;
      if ( total_time_spent > timeout_ms ) {
	perror( "write" );
	return -1;
      }
      uint64_t time_left = timeout_ms - total_time_spent;
      uint64_t sleep_time = 999;
      if ( time_left < sleep_time ) sleep_time = time_left;
      if ( iteration * 50 < sleep_time ) sleep_time = iteration * 50;
      fatal_assert( sleep_time > 0 );
      struct timespec req;
      req.tv_sec = 0;
      req.tv_nsec = sleep_time * 1000000;
      nanosleep( &req, NULL );
      continue;
    } else {
      perror( "write" );
    }
  }
  return 0;
}
