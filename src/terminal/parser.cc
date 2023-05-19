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

#include <assert.h>
#include <typeinfo>
#include <errno.h>
#include <wchar.h>
#include <stdint.h>

#include "parser.h"

const Parser::StateFamily Parser::family;

static void append_or_delete( Parser::ActionPointer act,
			      Parser::Actions &vec )
{
  assert( act );

  if ( !act->ignore() ) {
    vec.push_back( act );
  }
}

void Parser::Parser::input( wchar_t ch, Actions &ret )
{
  Transition tx = state->input( ch );

  if ( tx.next_state != NULL ) {
    append_or_delete( state->exit(), ret );
  }

  append_or_delete( tx.action, ret );

  if ( tx.next_state != NULL ) {
    append_or_delete( tx.next_state->enter(), ret );
    state = tx.next_state;
  }
}

Parser::UTF8Parser::UTF8Parser()
  : parser(), buf_len( 0 )
{
  assert( BUF_SIZE >= (size_t)MB_CUR_MAX );
  buf[0] = '\0';
}

void Parser::UTF8Parser::input( char c, Actions &ret )
{
  assert( buf_len < BUF_SIZE );

  /* 1-byte UTF-8 character, aka ASCII?  Cheat. */
  if ( buf_len == 0 && static_cast<unsigned char>(c) <= 0x7f ) {
    parser.input( static_cast<wchar_t>(c), ret );
    return;
  }

  buf[ buf_len++ ] = c;

  /* This function will only work in a UTF-8 locale. */
  wchar_t pwc;
  mbstate_t ps = mbstate_t();

  size_t total_bytes_parsed = 0;
  size_t orig_buf_len = buf_len;

  /* this routine is somewhat complicated in order to comply with
     Unicode 6.0, section 3.9, "Best Practices for using U+FFFD" */

  while ( total_bytes_parsed != orig_buf_len ) {
    assert( total_bytes_parsed < orig_buf_len );
    assert( buf_len > 0 );
    size_t bytes_parsed = mbrtowc( &pwc, buf, buf_len, &ps );

    /* this returns 0 when n = 0! */

    if ( bytes_parsed == 0 ) {
      /* character was NUL, accept and clear buffer */
      assert( buf_len == 1 );
      buf_len = 0;
      pwc = L'\0';
      bytes_parsed = 1;
    } else if ( bytes_parsed == (size_t) -1 ) {
      /* invalid sequence, use replacement character and try again with last char */
      assert( errno == EILSEQ );
      if ( buf_len > 1 ) {
	buf[ 0 ] = buf[ buf_len - 1 ];
	bytes_parsed = buf_len - 1;
	buf_len = 1;
      } else {
	buf_len = 0;
	bytes_parsed = 1;
      }
      pwc = (wchar_t) 0xFFFD;
    } else if ( bytes_parsed == (size_t) -2 ) {
      /* can't parse incomplete multibyte character */
      total_bytes_parsed += buf_len;
      continue;
    } else {
      /* Cast to unsigned for checks, because some
         platforms (e.g. ARM) use uint32_t as wchar_t,
         causing compiler warning on "pwc > 0" check. */
      const uint32_t pwcheck = pwc;

      if (pwcheck > 0x10FFFF)
      { /* outside Unicode range */
        pwc = (wchar_t)0xFFFD;
      }

      if ((pwcheck >= 0xD800) && (pwcheck <= 0xDFFF))
      { /* surrogate code point */
        /*
        A surrogate code point is a Unicode code point in the range 
        U+D800…U+DFFF. It is reserved by UTF-16 for a pair of 
        surrogate code units (a high surrogate followed by a low 
        surrogate) to "substitute" a supplementary character.

        For example, the Unicode code point U+1F339 (🌹) is a 
        supplementary character, which is encoded in UTF-16 as two 
        surrogate code units: 0xD83C (high surrogate) and 0xDF39 
        (low surrogate).

        🌹 is represented in UTF-8 as 0xF0 0x9F 0x8C 0xB9. UTF-8 
        is a variable-length character encoding, defined to encode 
        code points as 1 to 4 bytes, depending on the number of 
        significant binary bits in the code point value.

        Supplementary characters in Unicode are those assigned to 
        code points from U+10000 to U+10FFFF. In UTF-8, these 
        characters are all 4 bytes long. Therefore, there are no 
        Unicode supplementary characters longer than 4 bytes.

        The relationship between surrogate code units and 
        supplementary characters is as follows: supplementary 
        characters are those outside the Basic Multilingual Plane 
        (BMP), while surrogates are UTF-16 code values. In UTF-16, 
        a pair of surrogates (a high surrogate and a low surrogate) 
        is required to represent a supplementary character. The 
        range of high surrogates is U+D800 to U+DBFF, and the range 
        of low surrogates is U+DC00 to U+DFFF.

        I did an experiment in the MSYS2 environment. The emoji 
        character 📁 occupies 2 `wchar_t` or 4 `char`. The emoji 
        character ✏️ occupies 2 `wchar_t` or 6 `char`. I 
        organized my findings into a table, where the Display 
        column indicates whether it can be displayed normally, the 
        mbrtowc column shows the number of characters required and 
        the number of wide characters output by calling the mbrtowc 
        function. The pwccheck column is whether pwccheck is ok in 
        the code.

        | Emoji | Platform | `wchar_t` | `char` | Display | mbrtowc | pwccheck |
        | ----- | -------- | --------- | ------ | ------- | ------- | -------- |
        | 📁   | MSYS2    | 2         | 4      | ❌      | 3→1     | ❌      |
        | ✏️   | MSYS2    | 2         | 6      | ✔️      | 3→1     | ✔️      |
        | ❌   | MSYS2    | 1         | 3      | ✔️      | 3→1     | ✔️      |
        | 📁   | CentOS   | 1         | 4      | ✔️      | 4→1     | ✔️      |
        | ✏️   | CentOS   | 2         | 6      | ✔️      | 3→1     | ✔️      |
        | ❌   | CentOS   | 1         | 3      | ✔️      | 3→1     | ✔️      |

        Let’s summarize this table. Since the conversion process 
        is done gradually from small to large, we will encounter 
        this situation: after trying to convert a 3-character 
        length string to a wide character, it is located at a 
        surrogate code point, which is a surrogate code unit.

        So there is a problem here. The mbrtowc function converted 
        the first three characters of a Unicode supplementary 
        character, and got a wide character, which is the leading 
        code unit of the surrogate pair for the supplementary 
        character. This causes the remaining one character to be 
        impossible to convert to the correct data. This might be a 
        problem with mbrtowc under MSYS2, possibly because the 
        length of wchar_t on Windows is 2. According to the 
        background knowledge provided by Bing, a supplementary 
        character must be composed of 4 characters. Therefore, this 
        place should process 4 characters together and convert them 
        to 2 wide characters.

        Since the conversion process is from 4 characters to 2 wide 
        characters, we need to use the mbstowcs series of functions.
         */
        if (buf_len == 3 && bytes_parsed == 3) {
          total_bytes_parsed += buf_len;
          continue;
        }

        if (buf_len == 4 && bytes_parsed == 3) {
          wchar_t wstr[8];
          buf[buf_len] = '\0';
          size_t r = std::mbstowcs(wstr, buf, buf_len);
          if (r == 2) {
            bytes_parsed = buf_len;
          }
        }
        /*
          OS X unfortunately allows these sequences without EILSEQ, but
          they are ill-formed UTF-8 and we shouldn't repeat them to the
          user's terminal.
        */
        pwc = (wchar_t)0xFFFD;
      }

      /* parsed into pwc, accept */
      assert( bytes_parsed <= buf_len );
      memmove( buf, buf + bytes_parsed, buf_len - bytes_parsed );
      buf_len = buf_len - bytes_parsed;
    }

    parser.input( pwc, ret );

    total_bytes_parsed += bytes_parsed;

    if (bytes_parsed >= 4) {
      auto c = dynamic_cast<Print*>(ret.back().get());
      c->raw.append(buf, bytes_parsed);
    }
  }
}

Parser::Parser::Parser( const Parser &other )
  : state( other.state )
{}

Parser::Parser & Parser::Parser::operator=( const Parser &other )
{
  state = other.state;
  return *this;
}
