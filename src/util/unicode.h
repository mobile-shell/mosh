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

#ifndef MOSH_UNICODE_HPP
#define MOSH_UNICODE_HPP

#include <cstddef>
#include <cwchar>
#include <string>

/* Mosh passes a single Unicode code point around between the UTF-8 parser,
   the terminal emulator and the display.  On POSIX that code point rides in a
   wchar_t, which is 32 bits wide there.

   On Windows wchar_t is 16 bits (UTF-16), so it cannot hold anything above
   U+FFFF.  MSVC's mbrtowc() reports EILSEQ rather than producing a surrogate,
   which would turn every astral character -- emoji above all -- into U+FFFD.
   So on Windows the code point type is char32_t instead, and this header
   supplies the conversions that go with it.

   The three functions below have the same signature on both platforms, so
   the terminal code that calls them needs no conditional compilation.  POSIX
   builds keep the historical wchar_t path byte for byte. */

#ifdef _WIN32

using mosh_wchar_t = char32_t;
using mosh_wstring = std::u32string;
#define MOSH_L( x ) U##x
#define MOSH_MB_LEN_MAX 4

#else /* !_WIN32 */

#include <climits>

using mosh_wchar_t = wchar_t;
using mosh_wstring = std::wstring;
#define MOSH_L( x ) L##x
#define MOSH_MB_LEN_MAX MB_LEN_MAX

#endif /* _WIN32 */

/* UTF-8 -> one code point.  Same contract as mbrtowc(3), including the
   0 (NUL), (size_t)-1 (invalid sequence, errno=EILSEQ) and (size_t)-2
   (incomplete) returns that src/terminal/parser.cc relies on to implement
   Unicode 6.0 section 3.9, "Best Practices for using U+FFFD".

   The conversion state is carried for the sake of the POSIX implementation;
   the Windows one is stateless and ignores it. */
size_t mosh_mbrtowc( mosh_wchar_t* pwc, const char* s, size_t n, mbstate_t* ps );

/* Appends one code point to a string as UTF-8. */
void mosh_append_utf8( std::string& dest, mosh_wchar_t c );

/* Convert between UTF-8 bytes and the code-point string type.  Invalid input
   is replaced with U+FFFD rather than rejected: these are used for status
   messages, where losing a character beats losing the message. */
mosh_wstring mosh_widen( const std::string& s );
std::string mosh_narrow( const mosh_wstring& s );

/* Number of terminal columns occupied by a code point: 0 for combining marks
   and other zero-width characters, 1 for ordinary ones, 2 for East Asian Wide
   and Fullwidth, and -1 for control characters.

   On POSIX this is the system wcwidth(3).  On Windows the CRT has no such
   function at all, so unicode.cc asks the ICU that ships with the OS. */
int mosh_wcwidth( mosh_wchar_t wc );

#endif
