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

#ifndef SELECT_HPP
#define SELECT_HPP

#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <assert.h>

#include "fatal_assert.h"
#include "timestamp.h"

/* Convenience wrapper for pselect(2).

   Any signals blocked by calling sigprocmask() outside this code will still be
   received during Select::select().  So don't do that. */

class Select {
public:
  static Select &get_instance( void ) {
    /* COFU may or may not be thread-safe, depending on compiler */
    static Select instance;
    return instance;
  }

private:
  Select()
    : max_fd( -1 )
    /* These initializations are not used; they are just
       here to appease -Weffc++. */
    , all_fds( dummy_fd_set )
    , read_fds( dummy_fd_set )
    , empty_sigset( dummy_sigset )
    , consecutive_polls( 0 )
  {
    FD_ZERO( &all_fds );
    FD_ZERO( &read_fds );

    clear_got_signal();
    fatal_assert( 0 == sigemptyset( &empty_sigset ) );
  }

  void clear_got_signal( void )
  {
    memset( got_signal, 0, sizeof( got_signal ) );
  }

  /* not implemented */
  Select( const Select & );
  Select &operator=( const Select & );

public:
  void add_fd( int fd )
  {
    if ( fd > max_fd ) {
      max_fd = fd;
    }
    FD_SET( fd, &all_fds );
  }

  void clear_fds( void )
  {
    FD_ZERO( &all_fds );
  }

  static void add_signal( int signum )
  {
    fatal_assert( signum >= 0 );
    fatal_assert( signum <= MAX_SIGNAL_NUMBER );

    /* Block the signal so we don't get it outside of pselect(). */
    sigset_t to_block;
    fatal_assert( 0 == sigemptyset( &to_block ) );
    fatal_assert( 0 == sigaddset( &to_block, signum ) );
    fatal_assert( 0 == sigprocmask( SIG_BLOCK, &to_block, NULL ) );

    /* Register a handler, which will only be called when pselect()
       is interrupted by a (possibly queued) signal. */
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = &handle_signal;
    fatal_assert( 0 == sigfillset( &sa.sa_mask ) );
    fatal_assert( 0 == sigaction( signum, &sa, NULL ) );
  }

  /* timeout unit: milliseconds; negative timeout means wait forever */
  int select( int timeout )
  {
    memcpy( &read_fds,  &all_fds, sizeof( read_fds  ) );
    clear_got_signal();

    /* Rate-limit and warn about polls. */
    if ( verbose > 1 && timeout == 0 ) {
      fprintf( stderr, "%s: got poll (timeout 0)\n", __func__ );
    }
    if ( timeout == 0 && ++consecutive_polls >= MAX_POLLS ) {
      if ( verbose > 1 && consecutive_polls == MAX_POLLS ) {
	fprintf( stderr, "%s: got %d polls, rate limiting.\n", __func__, MAX_POLLS );
      }
      timeout = 1;
    } else if ( timeout != 0 && consecutive_polls ) {
      if ( verbose > 1 && consecutive_polls >= MAX_POLLS ) {
	fprintf( stderr, "%s: got %d consecutive polls\n", __func__, consecutive_polls );
      }
      consecutive_polls = 0;
    }

#ifdef HAVE_PSELECT
    struct timespec ts;
    struct timespec *tsp = NULL;

    if ( timeout >= 0 ) {
      ts.tv_sec  = timeout / 1000;
      ts.tv_nsec = 1000000 * (long( timeout ) % 1000);
      tsp = &ts;
    }

    int ret = ::pselect( max_fd + 1, &read_fds, NULL, NULL, tsp, &empty_sigset );
#else
    struct timeval tv;
    struct timeval *tvp = NULL;
    sigset_t old_sigset;

    if ( timeout >= 0 ) {
      tv.tv_sec  = timeout / 1000;
      tv.tv_usec = 1000 * (long( timeout ) % 1000);
      tvp = &tv;
    }

    int ret = sigprocmask( SIG_SETMASK, &empty_sigset, &old_sigset );
    if ( ret != -1 ) {
      ret = ::select( max_fd + 1, &read_fds, NULL, NULL, tvp );
      sigprocmask( SIG_SETMASK, &old_sigset, NULL );
    }
#endif

    if ( ret == 0 || ( ret == -1 && errno == EINTR ) ) {
      /* Look for and report Cygwin select() bug. */
      if ( ret == 0 ) {
	for ( int fd = 0; fd <= max_fd; fd++ ) {
	  if ( FD_ISSET( fd, &read_fds ) ) {
	    fprintf( stderr, "select(): nfds = 0 but read fd %d is set\n", fd );
	  }
	}
      }
      /* The user should process events as usual. */
      FD_ZERO( &read_fds );
      ret = 0;
    }

    freeze_timestamp();

    return ret;
  }

  bool read( int fd )
#if FD_ISSET_IS_CONST
    const
#endif
  {
    assert( FD_ISSET( fd, &all_fds ) );
    return FD_ISSET( fd, &read_fds );
  }

  /* This method consumes a signal notification. */
  bool signal( int signum )
  {
    fatal_assert( signum >= 0 );
    fatal_assert( signum <= MAX_SIGNAL_NUMBER );
    /* XXX This requires a guard against concurrent signals. */
    bool rv = got_signal[ signum ];
    got_signal[ signum ] = 0;
    return rv;
  }

  /* This method does not consume signal notifications. */
  bool any_signal( void ) const
  {
    bool rv = false;
    for (int i = 0; i < MAX_SIGNAL_NUMBER; i++) {
      rv |= got_signal[ i ];
    }
    return rv;
  }

  static void set_verbose( unsigned int s_verbose ) { verbose = s_verbose; }

private:
  static const int MAX_SIGNAL_NUMBER = 64;
  /* Number of 0-timeout selects after which we begin to think
   * something's wrong. */
  static const int MAX_POLLS = 10;

  static void handle_signal( int signum );

  int max_fd;

  /* We assume writes to these ints are atomic, though we also try to mask out
     concurrent signal handlers. */
  int got_signal[ MAX_SIGNAL_NUMBER + 1 ];

  fd_set all_fds, read_fds;

  sigset_t empty_sigset;

  static fd_set dummy_fd_set;
  static sigset_t dummy_sigset;
  int consecutive_polls;
  static unsigned int verbose;
};

#endif
