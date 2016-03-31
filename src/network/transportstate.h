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

#ifndef TRANSPORT_STATE_HPP
#define TRANSPORT_STATE_HPP

namespace Network {
  template <class State>
  class TimestampedState
  {
  public:
    uint64_t timestamp;
    uint64_t num;
    State state;
    
    TimestampedState( uint64_t s_timestamp, uint64_t s_num, const State &s_state )
      : timestamp( s_timestamp ), num( s_num ), state( s_state )
    {}

    /* For use with find_if, remove_if */
    bool num_eq( uint64_t v ) const { return num == v; }
    bool num_lt( uint64_t v ) const { return num <  v; }
  };
}

#endif
