/*
    Mosh: the mobile shell

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

#ifndef STATESYNC_RESIZE_HPP
#define STATESYNC_RESIZE_HPP

#include <cstddef>
#include <cstdint>

#include "src/terminal/parseraction.h"
#include "src/util/dos_assert.h"

namespace StateSync {
/* Unix terminal window-size ioctls expose row and column counts as 16-bit fields. */
static const int32_t MAX_TERMINAL_WIDTH = 65535;
static const int32_t MAX_TERMINAL_HEIGHT = 65535;
/* About one gigabyte of Terminal::Cell objects on systems with 64-bit pointers. */
static const uint64_t MAX_TERMINAL_CELLS = uint64_t( 1 ) << 24;

inline bool resize_dimensions_are_valid( const uint64_t width, const uint64_t height )
{
  return width > 0 && height > 0 && width <= MAX_TERMINAL_WIDTH && height <= MAX_TERMINAL_HEIGHT
         && width * height <= MAX_TERMINAL_CELLS;
}

inline Parser::Resize checked_resize_dimensions( const int32_t width, const int32_t height )
{
  dos_assert( width > 0 );
  dos_assert( height > 0 );
  dos_assert( resize_dimensions_are_valid( static_cast<uint64_t>( width ), static_cast<uint64_t>( height ) ) );

  return Parser::Resize( static_cast<size_t>( width ), static_cast<size_t>( height ) );
}

template<class ResizeMessage>
Parser::Resize checked_resize( const ResizeMessage& resize )
{
  dos_assert( resize.has_width() );
  dos_assert( resize.has_height() );

  return checked_resize_dimensions( resize.width(), resize.height() );
}
} // namespace StateSync

#endif
