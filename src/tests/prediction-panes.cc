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

/* Typing prediction on a screen split into side-by-side panes, the way
   tmux, vim and emacs draw them.  Local echo has to stay inside the pane
   that holds the cursor: the borders and the other panes never move when
   you type in one pane. */

#include <clocale>
#include <cstdio>
#include <string>

#include "src/frontend/terminaloverlay.h"
#include "src/statesync/completeterminal.h"
#include "src/util/locale_utils.h"

namespace {

/* A real layout: four panes of 95 columns, with tmux's default borders. */
const int WIDTH = 383;
const int HEIGHT = 6;
const int BORDERS[] = { 95, 191, 287 };
const std::string BAR = "\xE2\x94\x82"; /* U+2502, tmux's default vertical border */

const char* current_test = "";
int failures = 0;

/* Move the cursor; rows and columns count from 0. */
std::string at( int row, int col )
{
  char buf[32];
  snprintf( buf, sizeof buf, "\033[%d;%dH", row + 1, col + 1 );
  return buf;
}

/* Four panes.  Pane 2 (columns 96-190) has a shell prompt on row 2 with the
   cursor after it, at column 98.  Panes 3 and 4 have text on that row. */
std::string four_panes( const std::string& bar = BAR )
{
  std::string s;
  for ( int row = 0; row < HEIGHT; row++ ) {
    for ( int border : BORDERS ) {
      s += at( row, border ) + bar;
    }
  }
  s += at( 2, 192 ) + "pane three";
  s += at( 2, 288 ) + "pane four";
  s += at( 2, 96 ) + "$ ";
  return s;
}

/* A mosh client, talking to a server whose screen the test draws on directly. */
class Session
{
public:
  Terminal::Complete server;
  Overlay::PredictionEngine engine;
  Terminal::Framebuffer shown; /* what the user sees: the server's screen plus predictions */
  uint64_t sent;               /* number of the last state the client sent */

  Session( int width = WIDTH, int height = HEIGHT )
    : server( width, height ), engine(), shown( server.get_fb() ), sent( 0 )
  {
    /* A round trip of about 100 ms: predictions are shown, not underlined. */
    engine.set_send_interval( 50 );
  }

  /* What stmclient.cc does: copy the server's screen and draw the predictions on it. */
  void render( void )
  {
    Terminal::Framebuffer fb( server.get_fb() );
    engine.cull( fb );
    engine.apply( fb );
    shown = fb;
  }

  /* The user presses keys.  Each one goes to the server in a state of its own. */
  void type( const std::string& keys )
  {
    for ( char key : keys ) {
      engine.set_local_frame_sent( sent );
      engine.new_user_byte( key, shown );
      sent++;
      render();
    }
  }

  /* The server's screen changes, and the server acknowledges our states up to `upto`. */
  void reply( const std::string& output, uint64_t upto )
  {
    server.act( output );
    engine.set_local_frame_acked( upto );
    engine.set_local_frame_late_acked( upto );
    render();
  }

  std::string cell( int row, int col ) const
  {
    std::string s;
    shown.get_cell( row, col )->print_grapheme( s );
    return s;
  }
};

void expect_cell( const Session& s, int row, int col, const std::string& want, int line )
{
  const std::string got = s.cell( row, col );
  if ( got != want ) {
    fprintf( stderr,
             "%s:%d: %s: cell (%d,%d) shows \"%s\", expected \"%s\"\n",
             __FILE__,
             line,
             current_test,
             row,
             col,
             got.c_str(),
             want.c_str() );
    failures++;
  }
}

void expect_cursor( const Session& s, int row, int col, int line )
{
  const int got_row = s.shown.ds.get_cursor_row();
  const int got_col = s.shown.ds.get_cursor_col();
  if ( got_row != row || got_col != col ) {
    fprintf( stderr,
             "%s:%d: %s: cursor shown at (%d,%d), expected (%d,%d)\n",
             __FILE__,
             line,
             current_test,
             got_row,
             got_col,
             row,
             col );
    failures++;
  }
}

void expect_default_colours( const Session& s, int row, int col, int line )
{
  if ( !( s.shown.get_cell( row, col )->get_renditions() == Terminal::Renditions( 0 ) ) ) {
    fprintf( stderr,
             "%s:%d: %s: cell (%d,%d) is not drawn in the default colours\n",
             __FILE__,
             line,
             current_test,
             row,
             col );
    failures++;
  }
}

#define EXPECT_CELL( s, row, col, want ) expect_cell( s, row, col, want, __LINE__ )
#define EXPECT_CURSOR( s, row, col ) expect_cursor( s, row, col, __LINE__ )
#define EXPECT_DEFAULT_COLOURS( s, row, col ) expect_default_colours( s, row, col, __LINE__ )

/* Keys typed ahead of the server's echo are shown with the borders and the
   panes to the right where they are.  Unmodified mosh pushed them right. */
void test_borders_stay_put_while_keys_are_in_flight( void )
{
  /* tmux's single, heavy and double borders, and the ASCII one vim and emacs use */
  for ( const char* bar : { "\xE2\x94\x82", "\xE2\x94\x83", "\xE2\x95\x91", "|" } ) {
    Session s;
    s.reply( four_panes( bar ), 0 );
    s.type( "ab" );
    s.reply( "a", 1 ); /* the server has echoed only the first key so far */

    EXPECT_CELL( s, 2, 99, "b" );
    EXPECT_CELL( s, 2, 191, bar );
    EXPECT_CELL( s, 2, 192, "p" );
    EXPECT_CELL( s, 2, 288, "p" );
  }
}

/* Once the server confirms a key typed in a pane, the next key is echoed at
   once.  Unmodified mosh found its moved border contradicted, threw all
   predictions away and waited for the server again, so in every pane but
   the rightmost it gave no early echo at all. */
void test_echo_continues_after_a_confirmed_key( void )
{
  Session s;
  s.reply( four_panes(), 0 );
  s.type( "a" );
  s.reply( "a", 1 );
  s.type( "b" );

  EXPECT_CELL( s, 2, 99, "b" );
  EXPECT_CURSOR( s, 2, 100 );
}

/* Backspace pulls only the rest of the pane left; the border stays. */
void test_backspace_stays_inside_the_pane( void )
{
  Session s;
  s.reply( four_panes() + "hello", 0 ); /* "$ hello", cursor at column 103 */
  s.type( "x" );
  s.reply( "x", 1 );
  s.type( "\x7f" );

  EXPECT_CURSOR( s, 2, 103 );
  EXPECT_CELL( s, 2, 102, "o" );
  EXPECT_CELL( s, 2, 103, " " );
  EXPECT_CELL( s, 2, 190, " " );
  EXPECT_CELL( s, 2, 191, BAR );
  EXPECT_CELL( s, 2, 192, "p" );
}

/* The rightmost pane has no border on its right, so the rest of the line
   moves right as it always did. */
void test_rightmost_pane_shifts_the_rest_of_the_line( void )
{
  Session s;
  s.reply( four_panes() + at( 3, 288 ) + "$ tail" + at( 3, 290 ), 0 );
  s.type( "x" );
  s.reply( "xtail" + at( 3, 291 ), 1 );
  s.type( "y" );

  EXPECT_CELL( s, 3, 291, "y" );
  EXPECT_CELL( s, 3, 292, "t" );
  EXPECT_CELL( s, 3, 295, "l" );
}

/* A '|' on a full-width command line is text, not a border: nothing above
   or below it continues the line. */
void test_a_lone_bar_is_not_a_border( void )
{
  Session s( 80, 3 );
  s.reply( at( 1, 0 ) + "$ cat | wc" + at( 1, 5 ), 0 );
  s.type( "x" );
  s.reply( "x | wc" + at( 1, 6 ), 1 );
  s.type( "y" );

  EXPECT_CELL( s, 1, 6, "y" );
  EXPECT_CELL( s, 1, 8, "|" );
  EXPECT_CELL( s, 1, 10, "w" );
}

/* Enter moves the cursor to the pane's left edge on the next row, and the
   keys typed after it are echoed there. */
void test_enter_returns_to_the_pane_edge( void )
{
  Session s;
  s.reply( four_panes(), 0 );
  s.type( "a" );
  s.reply( "a", 1 );
  s.type( "\rzw" );
  s.reply( at( 3, 96 ) + "z", 3 ); /* the server has handled Enter and z, not w yet */

  EXPECT_CELL( s, 3, 97, "w" );
  EXPECT_CURSOR( s, 3, 98 );
}

/* Typing in the pane's last column wraps to the pane's left edge. */
void test_typing_wraps_at_the_pane_edge( void )
{
  Session s;
  s.reply( four_panes() + at( 2, 189 ), 0 );
  s.type( "abcd" );
  s.reply( "ab" + at( 3, 96 ) + "c", 3 ); /* tmux wrapped c to the pane's next row */

  EXPECT_CELL( s, 3, 97, "d" );
  EXPECT_CURSOR( s, 3, 98 );
}

/* A key typed in the pane's last column is not shown until the server
   confirms it, as in the screen's last column: an app may wrap there, or
   draw a wrap mark. */
void test_last_column_of_the_pane_waits_for_the_server( void )
{
  Session s;
  s.reply( four_panes() + at( 2, 189 ), 0 );
  s.type( "a" );
  s.reply( "a", 1 );
  s.type( "b" ); /* in column 190, the pane's last */

  EXPECT_CELL( s, 2, 190, " " );
  EXPECT_CURSOR( s, 2, 190 );
}

/* The tests below use experimental mode, which shows every prediction at
   once.  That lets them look at one key's prediction without first getting
   an earlier one confirmed. */

/* Inserting in a full pane line leaves the pane's last column alone: what
   ends up there is not known until the server says. */
void test_last_column_of_the_pane_is_not_guessed( void )
{
  Session s;
  s.engine.set_display_preference( Overlay::PredictionEngine::Experimental );
  s.reply( four_panes() + at( 3, 187 ) + "WXYZ" + at( 3, 188 ), 0 ); /* Z in column 190 */
  s.type( "q" );

  EXPECT_CELL( s, 3, 188, "q" );
  EXPECT_CELL( s, 3, 189, "X" );
  EXPECT_CELL( s, 3, 190, "Z" );
  EXPECT_CELL( s, 3, 191, BAR );
}

void test_right_arrow_stops_at_the_pane_edge( void )
{
  Session s;
  s.engine.set_display_preference( Overlay::PredictionEngine::Experimental );
  s.reply( four_panes() + at( 2, 190 ), 0 );
  s.type( "\033[C" );

  EXPECT_CURSOR( s, 2, 190 );
}

void test_left_arrow_stops_at_the_pane_edge( void )
{
  Session s;
  s.engine.set_display_preference( Overlay::PredictionEngine::Experimental );
  s.reply( four_panes() + at( 2, 96 ), 0 );
  s.type( "\033[D" );

  EXPECT_CURSOR( s, 2, 96 );
}

/* Backspace at the pane's left edge predicts nothing, as it does at column 0. */
void test_backspace_at_the_pane_edge_predicts_nothing( void )
{
  for ( bool overwrite : { false, true } ) {
    Session s;
    s.engine.set_display_preference( Overlay::PredictionEngine::Experimental );
    s.engine.set_predict_overwrite( overwrite );
    s.reply( four_panes() + at( 2, 96 ), 0 );
    s.type( "\x7f" );

    EXPECT_CURSOR( s, 2, 96 );
    EXPECT_CELL( s, 2, 95, BAR );
    EXPECT_CELL( s, 2, 96, "$" );
  }
}

/* A key typed at the pane's left edge takes the colours the server draws
   with, not those of the border to its left. */
void test_typing_at_the_pane_edge_ignores_the_border_colours( void )
{
  Session s;
  s.engine.set_display_preference( Overlay::PredictionEngine::Experimental );
  /* green borders, like the ones tmux draws around the active pane */
  s.reply( four_panes( "\033[32m" + BAR + "\033[m" ) + at( 3, 96 ), 0 );
  s.type( "a" );

  EXPECT_CELL( s, 3, 96, "a" );
  EXPECT_DEFAULT_COLOURS( s, 3, 96 );
}

/* Switched off (MOSH_PREDICTION_PANES=no), typing predicts as upstream mosh
   does: the whole rest of the row moves right, borders included. */
void test_switched_off_the_rest_of_the_row_moves( void )
{
  Session s;
  s.engine.set_predict_panes( false );
  s.reply( four_panes(), 0 );
  s.type( "ab" );
  s.reply( "a", 1 );

  EXPECT_CELL( s, 2, 191, " " );
  EXPECT_CELL( s, 2, 193, BAR );
}

/* Enter on the bottom row of the screen blanks only the pane's part of it. */
void test_enter_on_the_bottom_row_blanks_only_the_pane( void )
{
  Session s;
  s.engine.set_display_preference( Overlay::PredictionEngine::Experimental );
  s.reply( four_panes() + at( 5, 0 ) + "left" + at( 5, 192 ) + "right" + at( 5, 98 ), 0 );
  s.type( "\r" );

  EXPECT_CURSOR( s, 5, 96 );
  EXPECT_CELL( s, 5, 0, "l" );
  EXPECT_CELL( s, 5, 191, BAR );
  EXPECT_CELL( s, 5, 192, "r" );
}

} // namespace

int main( void )
{
  set_native_locale();
  if ( !is_utf8_locale() ) {
    setlocale( LC_ALL, "C.UTF-8" );
  }
  if ( !is_utf8_locale() ) {
    fprintf( stderr, "skipped: needs a UTF-8 locale\n" );
    return 77;
  }

#define TEST( name ) { #name, name }
  const struct
  {
    const char* name;
    void ( *run )( void );
  } tests[] = {
    TEST( test_borders_stay_put_while_keys_are_in_flight ),
    TEST( test_echo_continues_after_a_confirmed_key ),
    TEST( test_backspace_stays_inside_the_pane ),
    TEST( test_rightmost_pane_shifts_the_rest_of_the_line ),
    TEST( test_a_lone_bar_is_not_a_border ),
    TEST( test_enter_returns_to_the_pane_edge ),
    TEST( test_typing_wraps_at_the_pane_edge ),
    TEST( test_last_column_of_the_pane_waits_for_the_server ),
    TEST( test_last_column_of_the_pane_is_not_guessed ),
    TEST( test_right_arrow_stops_at_the_pane_edge ),
    TEST( test_left_arrow_stops_at_the_pane_edge ),
    TEST( test_backspace_at_the_pane_edge_predicts_nothing ),
    TEST( test_typing_at_the_pane_edge_ignores_the_border_colours ),
    TEST( test_enter_on_the_bottom_row_blanks_only_the_pane ),
    TEST( test_switched_off_the_rest_of_the_row_moves ),
  };
#undef TEST

  for ( const auto& test : tests ) {
    current_test = test.name;
    test.run();
  }

  if ( failures ) {
    fprintf( stderr, "%d check(s) failed\n", failures );
    return 1;
  }
  return 0;
}
