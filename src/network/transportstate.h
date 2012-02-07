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
    
    TimestampedState( uint64_t s_timestamp, uint64_t s_num, State &s_state )
      : timestamp( s_timestamp ), num( s_num ), state( s_state )
    {}
  };
}

#endif
