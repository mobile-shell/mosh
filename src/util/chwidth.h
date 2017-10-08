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

#ifndef CHWIDTH_H
#define CHWIDTH_H

#include <wchar.h>
#include <string>
#include <vector>

#include "shared.h"

class ChWidth
{
 public:  
  /* Range of expressible codepoints in Unicode. */
  static const size_t unicode_codespace = 0x110000;

 private:
  typedef std::vector<unsigned char> ChWidths;
  ChWidths widths;

 public:
  ChWidth();
  ChWidth( const ChWidth & widths );
  ChWidth & operator=( const ChWidth & widths );
  
  /* wcwidth, renamed. */
  int chwidth(wchar_t wc) const;

  /* Construct a difference set from this and a variant table. */
  std::string make_diff( const ChWidth& variant ) const;
  /* Apply a difference set to an existing table. */
  bool apply_diff( const std::string& diff );

  /* Retrieve the reference Unicode table embedded in all Mosh versions. */
  static std::string get_reference( void );
  /* Retrieve the Unicode table embedded in this version of Mosh.  It
     may be a later version of Unicode than the base table, or vary in
     other ways. */
  static std::string get_default( void );
  /* Retrieve a delta that sets all East Asian Width characters to wide.
     The delta is generated from the same Unicode data as the default table. */
  static std::string get_eaw_delta( void );
};

typedef shared::shared_ptr<ChWidth> ChWidthPtr;

#endif
