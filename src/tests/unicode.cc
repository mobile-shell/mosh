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

/* Exercises the UTF-8 codec and character-width table in src/util/unicode.cc.

   On Windows both are mosh's own code rather than the C library's, and a
   quiet mistake in either desynchronises the client's framebuffer from the
   server's.  This runs on POSIX too, where it checks that the wrappers
   around mbrtowc(3) and wcwidth(3) still behave the way the parser expects. */

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include "src/util/fatal_assert.h"
#include "src/util/locale_utils.h"
#include "src/util/unicode.h"

namespace {

size_t decode( const std::string& bytes, mosh_wchar_t* out )
{
  mbstate_t ps = mbstate_t();
  errno = 0;
  return mosh_mbrtowc( out, bytes.data(), bytes.size(), &ps );
}

void expect_roundtrip( mosh_wchar_t cp, size_t expected_len )
{
  std::string encoded;
  mosh_append_utf8( encoded, cp );
  fatal_assert( encoded.size() == expected_len );

  mosh_wchar_t decoded = 0;
  const size_t consumed = decode( encoded, &decoded );
  fatal_assert( consumed == expected_len );
  fatal_assert( decoded == cp );
}

void describe( const char* what, const std::string& bytes, size_t consumed )
{
  fprintf( stderr, "%s: <", what );
  for ( size_t i = 0; i < bytes.size(); i++ ) {
    fprintf( stderr, "%s%02X", i ? " " : "", static_cast<unsigned char>( bytes[i] ) );
  }
  fprintf( stderr, "> returned %zd\n", static_cast<ssize_t>( consumed ) );
}

void expect_invalid( const std::string& bytes )
{
  mosh_wchar_t decoded = 0;
  const size_t consumed = decode( bytes, &decoded );
  if ( consumed != static_cast<size_t>( -1 ) || errno != EILSEQ ) {
    describe( "expected invalid", bytes, consumed );
    fatal_assert( consumed == static_cast<size_t>( -1 ) );
    fatal_assert( errno == EILSEQ );
  }
}

void expect_incomplete( const std::string& bytes )
{
  mosh_wchar_t decoded = 0;
  const size_t consumed = decode( bytes, &decoded );
  if ( consumed != static_cast<size_t>( -2 ) ) {
    describe( "expected incomplete", bytes, consumed );
    fatal_assert( consumed == static_cast<size_t>( -2 ) );
  }
}

void expect_width( mosh_wchar_t cp, int expected )
{
  const int got = mosh_wcwidth( cp );
  if ( got != expected ) {
    fprintf( stderr, "wcwidth(U+%04X) = %d, expected %d\n", static_cast<unsigned>( cp ), got, expected );
    fatal_assert( got == expected );
  }
}

} /* namespace */

int main( void )
{
#ifndef _WIN32
  /* The POSIX side of unicode.cc delegates to mbrtowc/wcrtomb, which only
     handle UTF-8 in a UTF-8 locale; mosh itself calls set_native_locale() on
     startup.  Skip the way the other locale-dependent tests do when the
     environment cannot provide one.  The Windows implementation does its own
     conversion and does not consult the locale at all. */
  set_native_locale();
  if ( !is_utf8_locale() ) {
    fprintf( stderr, "unicode: no UTF-8 locale available, skipping\n" );
    return 77;
  }
#endif

  /* ---- encode/decode round trips, one per UTF-8 length ---- */
  expect_roundtrip( 'A', 1 );      /* ASCII                     */
  expect_roundtrip( 0x00E9, 2 );   /* e with acute              */
  expect_roundtrip( 0x0416, 2 );   /* Cyrillic ZHE              */
  expect_roundtrip( 0x4E16, 3 );   /* CJK                       */
  expect_roundtrip( 0x1F600, 4 );  /* grinning face             */
  expect_roundtrip( 0x10FFFF, 4 ); /* last legal code point     */

  /* A NUL is reported as zero bytes consumed, which parser.cc relies on. */
  {
    mosh_wchar_t decoded = 0xFFFF;
    const std::string nul( 1, '\0' );
    fatal_assert( decode( nul, &decoded ) == 0 );
    fatal_assert( decoded == 0 );
  }

  /* ---- multi-character strings ---- */
  {
    const std::string mixed = "a\xD0\x96\xE4\xB8\x96\xF0\x9F\x98\x80";
    const mosh_wstring wide = mosh_widen( mixed );
    fatal_assert( wide.size() == 4 );
    fatal_assert( wide[0] == 'a' );
    fatal_assert( wide[1] == 0x0416 );
    fatal_assert( wide[2] == 0x4E16 );
    fatal_assert( wide[3] == 0x1F600 );
    fatal_assert( mosh_narrow( wide ) == mixed );
  }

  /* ---- ill-formed input ---- */
  expect_invalid( "\x80" );             /* stray continuation byte   */
  expect_invalid( "\xC0\xAF" );         /* overlong '/'              */
  expect_invalid( "\xE0\x80\xAF" );     /* overlong, three bytes     */
  expect_invalid( "\xED\xA0\x80" );     /* UTF-16 surrogate D800     */
  expect_invalid( "\xE4\x28\xB8" );     /* bad continuation byte     */
  expect_invalid( "\xFE\x80\x80\x80" ); /* never a lead byte         */

  /* ---- out of Unicode range, but still decoded ----

     Client and server parse the same host bytes independently, so the
     decoder has to accept exactly what the server's does.  glibc still
     implements the original five- and six-byte UTF-8; the resulting code
     points get width -1 and the emulator drops them, which is what makes
     the two sides agree. Rejecting them here would draw U+FFFD on one side
     and nothing on the other. */
  {
    mosh_wchar_t decoded = 0;
    fatal_assert( decode( "\xF4\x90\x80\x80", &decoded ) == 4 ); /* U+110000 */
    fatal_assert( decoded == 0x110000 );
    fatal_assert( mosh_wcwidth( decoded ) == -1 );

    fatal_assert( decode( "\xF7\xBF\xBF\xBF", &decoded ) == 4 ); /* U+1FFFFF */
    fatal_assert( decoded == 0x1FFFFF );
    fatal_assert( mosh_wcwidth( decoded ) == -1 );

    fatal_assert( decode( "\xF8\x88\x80\x80\x80", &decoded ) == 5 ); /* U+200000 */
    fatal_assert( decoded == 0x200000 );
    fatal_assert( mosh_wcwidth( decoded ) == -1 );
  }

  /* ---- truncated but so far valid ---- */
  expect_incomplete( "\xD0" );
  expect_incomplete( "\xE4\xB8" );
  expect_incomplete( "\xF0\x9F\x98" );
  expect_incomplete( "\xF7\xBF" );

  /* ---- widths ---- */
  expect_width( 'A', 1 );
  expect_width( 0x00E9, 1 );
  expect_width( 0x0416, 1 );
  expect_width( 0x4E16, 2 );  /* CJK ideograph             */
  expect_width( 0xFF21, 2 );  /* fullwidth Latin A         */
  expect_width( 0x3042, 2 );  /* hiragana A                */
  expect_width( 0x0301, 0 );  /* combining acute accent    */
  expect_width( 0x0483, 0 );  /* combining Cyrillic titlo  */
  expect_width( 0x200B, 0 );  /* zero width space          */
  expect_width( 0x11A8, 0 );  /* Hangul jongseong kiyeok   */
  expect_width( 0x0000, 0 );  /* NUL                       */
  expect_width( 0x0007, -1 ); /* BEL                       */
  expect_width( 0x001B, -1 ); /* ESC                       */
  expect_width( 0x007F, -1 ); /* DEL                       */

  printf( "unicode: all checks passed\n" );
  return 0;
}
