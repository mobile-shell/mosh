/*
    Mosh: the mobile shell
    Copyright 2017 Keith Winstein

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

#include <chwidth.h>

static std::string& get_widths( void )
{
  static std::string widths( chwidth_reference_table );
  return widths;
}

void chwidth_set_base( const std::string& new_widths )
{
  std::string& widths = get_widths();
  widths = new_widths;
}

bool chwidth_set_overlay( const std::string& overlay )
{
  std::string& widths = get_widths();
  std::string workingwidths( widths );
  std::string::const_iterator ib = overlay.begin();
  std::string::const_iterator ie = overlay.end();
  for ( std::string::const_iterator i = ib; i < ie; i++ ) {
    switch( *i ) {
    case '=':
      continue;
    case '-':
    case '0':
    case '1':
    case '2':
      {
	size_t ix = i - ib;
	if ( ix < workingwidths.size()) {
	  workingwidths.at( ix ) = *i;
	} else {
	  return false;
	}
      }
      break;
    // XXX 'u': ?
    default:
      return false;
    }
  }
  widths = workingwidths;
  return true;
}

std::string chwidth_make_overlay( const std::string& base, const std::string& update )
{
  size_t overlaylen = std::min( base.size(), update.size());
  std::string overlay;
  overlay.reserve( overlaylen );
  for ( size_t i = 0; i < overlaylen; i++ ) {
    char b = base.at( i );
    char u = update.at( i );
    if ( b == u ) {
      overlay.push_back( '=' );
    } else {
      overlay.push_back( u );
    }
  }
  return overlay;
}

int chwidth( wchar_t wc )
{
  const std::string& widths = get_widths();
  if (static_cast<size_t>( wc) > widths.size()) {
    return -1;
  }
  char ch = widths.at( wc );
  switch ( ch ) {
  case '0':
    return 0;
  case '1':
    return 1;
  case '2':
    return 2;
  case '-':
  default:
    return -1;
  }
}

const char* chwidth_get_reference( void )
{
  return chwidth_reference_table;
}

const char* chwidth_get_default( void )
{
  return chwidth_default_table;
}

std::string chwidth_get_working(  void )
{
  return get_widths();
}
