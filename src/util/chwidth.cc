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

#include <sstream>

ChWidth::ChWidth() : widths( unicode_codespace, '-' ) {}

ChWidth::ChWidth( const ChWidth & other ) { widths = other.widths; }

ChWidth & ChWidth::operator=( const ChWidth & other ) { widths = other.widths; return *this; }


bool ChWidth::apply_diff( const std::string& overlay )
{
  ChWidths workingwidths( widths );
  std::istringstream lines( overlay );
  size_t run_start = 0;
  std::string lineString;
  while ( std::getline( lines, lineString )) {
    /* Strip comments. */
    std::string::size_type pos = lineString.find( '#' );
    if ( pos != std::string::npos ) {
      lineString.resize( pos );
    }
    /* Skip empty lines. */
    if ( lineString.empty() ) {
      continue;
    }
    /* Get run length and type. */
    std::istringstream line( lineString );
    size_t run_len;
    char run_type;
    line >> run_len >> run_type;
    if ( !line ) {
      return false;
    }
    switch ( run_type ) {
    case '=':
      break;
    case '-':
    case '0':
    case '1':
    case '2':
      if (run_start + run_len > workingwidths.size()) {
	return false;
      }
      for (size_t i = run_start; i < run_start + run_len; i++) {
	workingwidths.at(i) = run_type;
      }
      break;
    // XXX case 'u': ?
    default:
      return false;
    }
    run_start += run_len;
  }
  widths = workingwidths;
  return true;
}

std::string ChWidth::make_diff( const ChWidth& variant ) const
{
  size_t comparelen = std::min( widths.size(), variant.widths.size());
  std::ostringstream diff;
  size_t run_start = 0;
  char run_type = '=';
  for (size_t i = 0; i < comparelen; i++ ) {
    char b = widths.at( i );
    char v = variant.widths.at( i );
    char new_type = b == v ? '=' : v;
    if (run_type != new_type) {
      if (i > run_start) {
	diff << i - run_start << ' ' << run_type << std::endl;
      }
      run_type = new_type;
      run_start = i;
    }
  }
  if (run_start != comparelen) {
    diff << comparelen - run_start << ' ' << run_type << std::endl;
  }
  return diff.str();
}

int ChWidth::chwidth( wchar_t wc ) const
{
  if (static_cast<size_t>( wc ) >= widths.size()) {
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
