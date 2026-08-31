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

#include "src/util/unicode.h"

#include <cerrno>
#include <cstdint>

/* ---- shared between platforms ---- */

mosh_wstring mosh_widen( const std::string& s )
{
  mosh_wstring ret;
  mbstate_t ps = mbstate_t();

  size_t i = 0;
  while ( i < s.size() ) {
    mosh_wchar_t wc = 0;
    size_t consumed = mosh_mbrtowc( &wc, s.data() + i, s.size() - i, &ps );

    if ( consumed == static_cast<size_t>( -1 ) || consumed == static_cast<size_t>( -2 ) ) {
      wc = 0xFFFD;
      consumed = 1;
      ps = mbstate_t();
    } else if ( consumed == 0 ) {
      consumed = 1; /* embedded NUL */
    }

    ret.push_back( wc );
    i += consumed;
  }

  return ret;
}

std::string mosh_narrow( const mosh_wstring& s )
{
  std::string ret;
  for ( mosh_wstring::const_iterator it = s.begin(); it != s.end(); ++it ) {
    mosh_append_utf8( ret, *it );
  }
  return ret;
}

#ifdef _WIN32

/* Character width and general category come from the ICU that ships with
   Windows itself (icu.dll, present since Windows 10 1703).  Using the OS
   tables rather than a table compiled into mosh keeps the client agreeing
   with the glibc wcwidth(3) on the server about how wide a character is --
   and a disagreement there desynchronises the two framebuffers, which is
   exactly the class of bug that is miserable to chase down. */
#include <icu.h>

size_t mosh_mbrtowc( mosh_wchar_t* pwc, const char* s, size_t n, mbstate_t* ps )
{
  /* This decoder is stateless: parser.cc always re-presents the whole
     pending buffer, so there is nothing to carry between calls. */
  (void)ps;

  if ( n == 0 ) {
    /* No input: by definition an incomplete character. */
    return static_cast<size_t>( -2 );
  }

  const unsigned char* u = reinterpret_cast<const unsigned char*>( s );
  const unsigned char lead = u[0];

  size_t len;
  mosh_wchar_t cp;

  /* The acceptance range here deliberately matches glibc rather than the
     restricted UTF-8 of the current standard.  Client and server each parse
     the same host byte stream on their own -- hostinput.proto carries raw
     bytes -- so a byte sequence one side accepts and the other rejects
     desynchronises the two framebuffers.  glibc still decodes the original
     five- and six-byte forms; everything it produces above U+10FFFF gets
     width -1 from wcwidth(3) and is dropped by the emulator, and this
     implementation reaches the same outcome by the same route. */
  if ( lead < 0x80 ) {
    len = 1;
    cp = lead;
  } else if ( lead < 0xC2 ) {
    /* 0x80-0xBF is a stray continuation byte; 0xC0 and 0xC1 could only ever
       introduce an overlong encoding of ASCII. */
    errno = EILSEQ;
    return static_cast<size_t>( -1 );
  } else if ( lead < 0xE0 ) {
    len = 2;
    cp = lead & 0x1F;
  } else if ( lead < 0xF0 ) {
    len = 3;
    cp = lead & 0x0F;
  } else if ( lead < 0xF8 ) {
    len = 4;
    cp = lead & 0x07;
  } else if ( lead < 0xFC ) {
    len = 5;
    cp = lead & 0x03;
  } else if ( lead < 0xFE ) {
    len = 6;
    cp = lead & 0x01;
  } else {
    /* 0xFE and 0xFF are not lead bytes in any form of UTF-8. */
    errno = EILSEQ;
    return static_cast<size_t>( -1 );
  }

  /* Check the continuation bytes that are present before reporting a
     truncated character, so an invalid sequence is never mistaken for one
     that is merely incomplete. */
  const size_t avail = ( n < len ) ? n : len;
  for ( size_t i = 1; i < avail; i++ ) {
    if ( ( u[i] & 0xC0 ) != 0x80 ) {
      errno = EILSEQ;
      return static_cast<size_t>( -1 );
    }
    cp = ( cp << 6 ) | ( u[i] & 0x3F );
  }

  if ( n < len ) {
    return static_cast<size_t>( -2 );
  }

  /* Overlong forms and UTF-16 surrogates are rejected; values above
     U+10FFFF are not, because glibc accepts them and the emulator discards
     them later on width. */
  static const mosh_wchar_t min_for_len[7] = { 0, 0, 0x80, 0x800, 0x10000, 0x200000, 0x4000000 };
  if ( cp < min_for_len[len] || ( cp >= 0xD800 && cp <= 0xDFFF ) ) {
    errno = EILSEQ;
    return static_cast<size_t>( -1 );
  }

  if ( pwc ) {
    *pwc = cp;
  }

  /* mbrtowc(3) reports a NUL character as zero bytes consumed, and
     parser.cc depends on that. */
  return ( cp == 0 ) ? 0 : len;
}

void mosh_append_utf8( std::string& dest, mosh_wchar_t c )
{
  if ( c > 0x10FFFF || ( c >= 0xD800 && c <= 0xDFFF ) ) {
    return; /* not representable; drop it rather than emit invalid UTF-8 */
  }

  if ( c < 0x80 ) {
    dest.push_back( static_cast<char>( c ) );
  } else if ( c < 0x800 ) {
    dest.push_back( static_cast<char>( 0xC0 | ( c >> 6 ) ) );
    dest.push_back( static_cast<char>( 0x80 | ( c & 0x3F ) ) );
  } else if ( c < 0x10000 ) {
    dest.push_back( static_cast<char>( 0xE0 | ( c >> 12 ) ) );
    dest.push_back( static_cast<char>( 0x80 | ( ( c >> 6 ) & 0x3F ) ) );
    dest.push_back( static_cast<char>( 0x80 | ( c & 0x3F ) ) );
  } else {
    dest.push_back( static_cast<char>( 0xF0 | ( c >> 18 ) ) );
    dest.push_back( static_cast<char>( 0x80 | ( ( c >> 12 ) & 0x3F ) ) );
    dest.push_back( static_cast<char>( 0x80 | ( ( c >> 6 ) & 0x3F ) ) );
    dest.push_back( static_cast<char>( 0x80 | ( c & 0x3F ) ) );
  }
}

int mosh_wcwidth( mosh_wchar_t wc )
{
  const UChar32 c = static_cast<UChar32>( wc );

  /* wcwidth(3) reports NUL as zero-width and every other control character
     as -1. */
  if ( c == 0 ) {
    return 0;
  }
  if ( c < 0x20 || ( c >= 0x7F && c < 0xA0 ) ) {
    return -1;
  }
  if ( c > 0x10FFFF || ( c >= 0xD800 && c <= 0xDFFF ) ) {
    return -1;
  }

  /* Hangul Jamo medial vowels and final consonants compose onto the
     preceding syllable.  ICU classifies them as ordinary letters of neutral
     width, so they need calling out explicitly, as they do in glibc. */
  if ( c >= 0x1160 && c <= 0x11FF ) {
    return 0;
  }
  if ( c == 0x200B ) { /* ZERO WIDTH SPACE */
    return 0;
  }

  switch ( u_charType( c ) ) {
    case U_NON_SPACING_MARK:
    case U_ENCLOSING_MARK:
      return 0;
    case U_FORMAT_CHAR:
      /* SOFT HYPHEN is the one format character that occupies a column. */
      return ( c == 0x00AD ) ? 1 : 0;
    default:
      break;
  }

  const int32_t eaw = u_getIntPropertyValue( c, UCHAR_EAST_ASIAN_WIDTH );
  if ( eaw == U_EA_WIDE || eaw == U_EA_FULLWIDTH ) {
    return 2;
  }

  return 1;
}

#else /* !_WIN32 */

size_t mosh_mbrtowc( mosh_wchar_t* pwc, const char* s, size_t n, mbstate_t* ps )
{
  return mbrtowc( pwc, s, n, ps );
}

void mosh_append_utf8( std::string& dest, mosh_wchar_t c )
{
  /* ASCII?  Cheat. */
  if ( static_cast<uint32_t>( c ) <= 0x7f ) {
    dest.push_back( static_cast<char>( c ) );
    return;
  }

  mbstate_t ps = mbstate_t();
  char tmp[MOSH_MB_LEN_MAX];
  size_t ignore = wcrtomb( NULL, 0, &ps );
  (void)ignore;
  size_t len = wcrtomb( tmp, c, &ps );
  if ( len == static_cast<size_t>( -1 ) ) {
    return;
  }
  dest.append( tmp, len );
}

int mosh_wcwidth( mosh_wchar_t wc )
{
  return wcwidth( wc );
}

#endif /* _WIN32 */
