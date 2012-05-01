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

#ifndef SELECT_HPP
#define SELECT_HPP

#include <string.h>
#include <sys/select.h>

/* We don't need these on POSIX.1-2001 systems, but there's little reason
   not to include them. */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

static fd_set dummy_fd_set;

/* Convenience wrapper for select(2). */
class Select {
public:
  Select()
    : max_fd( -1 )

    /* These initializations are not used; they are just
       here to appease -Weffc++. */
    , all_fds( dummy_fd_set )
    , read_fds( dummy_fd_set )
    , error_fds( dummy_fd_set )
  {
    FD_ZERO( &all_fds );
    FD_ZERO( &read_fds );
    FD_ZERO( &error_fds );
  }

  void add_fd( int fd )
  {
    if ( fd > max_fd ) {
      max_fd = fd;
    }
    FD_SET( fd, &all_fds );
  }

  int select( int timeout )
  {
    memcpy( &read_fds,  &all_fds, sizeof( read_fds  ) );
    memcpy( &error_fds, &all_fds, sizeof( error_fds ) );

    struct timeval tv;
    struct timeval *tvp = NULL;

    if ( timeout >= 0 ) {
      // timeout in milliseconds
      tv.tv_sec  = timeout / 1000;
      tv.tv_usec = 1000 * (timeout % 1000);
      tvp = &tv;
    }
    // negative timeout means wait forever

    return ::select( max_fd + 1, &read_fds, NULL, &error_fds, tvp );
  }

  bool read( int fd ) const
  {
    return FD_ISSET( fd, &read_fds );
  }

  bool error( int fd ) const
  {
    return FD_ISSET( fd, &error_fds );
  }

private:
  int max_fd;
  fd_set all_fds, read_fds, error_fds;
};

#endif
