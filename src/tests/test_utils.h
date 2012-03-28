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

#ifndef TEST_UTILS_HPP
#define TEST_UTILS_HPP

#include <string>

#include "crypto.h"

#define DUMP_NAME_FMT "%-10s "

void hexdump( const void *buf, size_t len, const char *name );
void hexdump( const Crypto::AlignedBuffer &buf, const char *name );
void hexdump( const std::string &buf, const char *name );

#endif
