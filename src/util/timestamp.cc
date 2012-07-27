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

#include "timestamp.h"

#include <errno.h>

#if HAVE_CLOCK_GETTIME
 #include <time.h>
#elif HAVE_MACH_ABSOLUTE_TIME
 #include <mach/mach_time.h>
#elif HAVE_GETTIMEOFDAY
 #include <sys/time.h>
#endif

static uint64_t millis_cache = -1;

uint64_t frozen_timestamp( void )
{
  if ( millis_cache == uint64_t( -1 ) ) {
    freeze_timestamp();
  }

  return millis_cache;
}

void freeze_timestamp( void )
{
#if HAVE_CLOCK_GETTIME
  struct timespec tp;

  if ( clock_gettime( CLOCK_MONOTONIC, &tp ) < 0 ) {
    /* did not succeed */
  } else {
    uint64_t millis = tp.tv_nsec / 1000000;
    millis += uint64_t( tp.tv_sec ) * 1000;

    millis_cache = millis;
    return;
  }
#elif HAVE_MACH_ABSOLUTE_TIME
  static mach_timebase_info_data_t s_timebase_info;

  if (s_timebase_info.denom == 0) {
    mach_timebase_info(&s_timebase_info);
  }

  // NB: mach_absolute_time() returns "absolute time units"
  // We need to apply a conversion to get milliseconds.
  millis_cache = ((mach_absolute_time() * s_timebase_info.numer) / (1000000 * s_timebase_info.denom));
  return;								    
#elif HAVE_GETTIMEOFDAY
  // NOTE: If time steps backwards, timeouts may be confused.
  struct timeval tv;
  if ( gettimeofday(&tv, NULL) ) {
    perror( "gettimeofday" );
  } else {
    uint64_t millis = tv.tv_usec / 1000;
    millis += uint64_t( tv.tv_sec ) * 1000;

    millis_cache = millis;
    return;
  }
#else
# error "Don't know how to get a timestamp on this platform"
#endif
}
