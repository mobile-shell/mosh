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

#include "select.h"

Select *Select::instance = NULL;

fd_set Select::dummy_fd_set;

sigset_t Select::dummy_sigset;

void Select::handle_signal( int signum )
{
  fatal_assert( signum >= 0 );
  fatal_assert( signum <= MAX_SIGNAL_NUMBER );

  Select &sel = get_instance();
  sel.got_signal[ signum ] = 1;
  sel.got_any_signal = 1;
}
