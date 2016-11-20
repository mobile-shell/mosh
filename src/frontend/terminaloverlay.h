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

#ifndef TERMINAL_OVERLAY_HPP
#define TERMINAL_OVERLAY_HPP

#include "terminalframebuffer.h"
#include "network.h"
#include "transportsender.h"
#include "parser.h"

#include <vector>
#include <limits.h>

namespace Overlay {
  using namespace Terminal;
  using namespace Network;
  using std::deque;
  using std::list;
  using std::vector;
  using std::wstring;

  enum Validity {
    Pending,
    Correct,
    CorrectNoCredit,
    IncorrectOrExpired,
    Inactive
  };

  class ConditionalOverlay {
  public:
    uint64_t expiration_frame;
    int col;
    bool active; /* represents a prediction at all */
    uint64_t tentative_until_epoch; /* when to show */
    uint64_t prediction_time; /* used to find long-pending predictions */

    ConditionalOverlay( uint64_t s_exp, int s_col, uint64_t s_tentative )
      : expiration_frame( s_exp ), col( s_col ),
	active( false ),
	tentative_until_epoch( s_tentative ), prediction_time( uint64_t( -1 ) )
    {}

    virtual ~ConditionalOverlay() {}

    bool tentative( uint64_t confirmed_epoch ) const { return tentative_until_epoch > confirmed_epoch; }
    void reset( void ) { expiration_frame = tentative_until_epoch = -1; active = false; }
    void expire( uint64_t s_exp, uint64_t now )
    {
      expiration_frame = s_exp; prediction_time = now;
    }
  };

  class ConditionalCursorMove : public ConditionalOverlay {
  public:
    int row;

    void apply( Framebuffer &fb, uint64_t confirmed_epoch ) const;

    Validity get_validity( const Framebuffer &fb, uint64_t early_ack, uint64_t late_ack ) const;

    ConditionalCursorMove( uint64_t s_exp, int s_row, int s_col, uint64_t s_tentative )
      : ConditionalOverlay( s_exp, s_col, s_tentative ), row( s_row )
    {}
  };

  class ConditionalOverlayCell : public ConditionalOverlay {
  public:
    Cell replacement;
    bool unknown;

    vector<Cell> original_contents; /* we don't give credit for correct predictions
				       that match the original contents */

    void apply( Framebuffer &fb, uint64_t confirmed_epoch, int row, bool flag ) const;
    Validity get_validity( const Framebuffer &fb, int row, uint64_t early_ack, uint64_t late_ack ) const;

    ConditionalOverlayCell( uint64_t s_exp, int s_col, uint64_t s_tentative )
      : ConditionalOverlay( s_exp, s_col, s_tentative ),
	replacement( 0 ),
	unknown( false ),
	original_contents()
    {}

    void reset( void ) { unknown = false; original_contents.clear(); ConditionalOverlay::reset(); }
    void reset_with_orig( void ) {
      if ( (!active) || unknown ) {
	reset();
	return;
      }

      original_contents.push_back( replacement );
      ConditionalOverlay::reset();
    }
  };

  class ConditionalOverlayRow {
  public:
    int row_num;

    typedef vector<ConditionalOverlayCell> overlay_cells_type;
    overlay_cells_type overlay_cells;

    void apply( Framebuffer &fb, uint64_t confirmed_epoch, bool flag ) const;

    /* For use with find_if */
    bool row_num_eq( int v ) const { return row_num == v; }

    ConditionalOverlayRow( int s_row_num ) : row_num( s_row_num ), overlay_cells() {}
  };

  /* the various overlays */
  class NotificationEngine {
  private:
    uint64_t last_word_from_server;
    uint64_t last_acked_state;
    string escape_key_string;
    wstring message;
    bool message_is_network_error;
    uint64_t message_expiration;
    bool show_quit_keystroke;

    bool server_late( uint64_t ts ) const { return (ts - last_word_from_server) > 6500; }
    bool reply_late( uint64_t ts ) const { return (ts - last_acked_state) > 10000; }
    bool need_countup( uint64_t ts ) const { return server_late( ts ) || reply_late( ts ); }

  public:
    void adjust_message( void );
    void apply( Framebuffer &fb ) const;
    const wstring &get_notification_string( void ) const { return message; }
    void server_heard( uint64_t s_last_word ) { last_word_from_server = s_last_word; }
    void server_acked( uint64_t s_last_acked ) { last_acked_state = s_last_acked; }
    int wait_time( void ) const;

    void set_notification_string( const wstring &s_message, bool permanent = false, bool s_show_quit_keystroke = true )
    {
      message = s_message;
      if ( permanent ) {
        message_expiration = -1;
      } else {
        message_expiration = timestamp() + 1000;
      }
      message_is_network_error = false;
      show_quit_keystroke = s_show_quit_keystroke;
    }

    void set_escape_key_string( const string &s_name )
    {
      char tmp[ 128 ];
      snprintf( tmp, sizeof tmp, " [To quit: %s .]", s_name.c_str() );
      escape_key_string = tmp;
    }

    void set_network_error( const std::string &s )
    {
      wchar_t tmp[ 128 ];
      swprintf( tmp, 128, L"%s", s.c_str() );

      message = tmp;
      message_is_network_error = true;
      message_expiration = timestamp() + Network::ACK_INTERVAL + 100;
    }

    void clear_network_error()
    {
      if ( message_is_network_error ) {
	message_expiration = std::min( message_expiration, timestamp() + 1000 );
      }
    }

    NotificationEngine();
  };

  class PredictionEngine {
  private:
    static const uint64_t SRTT_TRIGGER_LOW = 20; /* <= ms cures SRTT trigger to show predictions */
    static const uint64_t SRTT_TRIGGER_HIGH = 30; /* > ms starts SRTT trigger */

    static const uint64_t FLAG_TRIGGER_LOW = 50; /* <= ms cures flagging */
    static const uint64_t FLAG_TRIGGER_HIGH = 80; /* > ms starts flagging */

    static const uint64_t GLITCH_THRESHOLD = 250; /* prediction outstanding this long is glitch */
    static const uint64_t GLITCH_REPAIR_COUNT = 10; /* non-glitches required to cure glitch trigger */
    static const uint64_t GLITCH_REPAIR_MININTERVAL = 150; /* required time in between non-glitches */

    static const uint64_t GLITCH_FLAG_THRESHOLD = 5000; /* prediction outstanding this long => underline */

    char last_byte;
    Parser::UTF8Parser parser;

    typedef list<ConditionalOverlayRow> overlays_type;
    overlays_type overlays;

    typedef list<ConditionalCursorMove> cursors_type;
    cursors_type cursors;

    typedef ConditionalOverlayRow::overlay_cells_type overlay_cells_type;

    uint64_t local_frame_sent, local_frame_acked, local_frame_late_acked;

    ConditionalOverlayRow & get_or_make_row( int row_num, int num_cols );

    uint64_t prediction_epoch;
    uint64_t confirmed_epoch;

    void become_tentative( void );

    void newline_carriage_return( const Framebuffer &fb );

    bool flagging; /* whether we are underlining predictions */
    bool srtt_trigger; /* show predictions because of slow round trip time */
    unsigned int glitch_trigger; /* show predictions temporarily because of long-pending prediction */
    uint64_t last_quick_confirmation;

    ConditionalCursorMove & cursor( void ) { assert( !cursors.empty() ); return cursors.back(); }

    void kill_epoch( uint64_t epoch, const Framebuffer &fb );

    void init_cursor( const Framebuffer &fb );

    unsigned int send_interval;

    int last_height, last_width;

  public:
    enum DisplayPreference {
      Always,
      Never,
      Adaptive,
      Experimental
    };

  private:
    DisplayPreference display_preference;

    bool active( void ) const;

    bool timing_tests_necessary( void ) const {
      /* Are there any timing-based triggers that haven't fired yet? */
      return !( glitch_trigger && flagging );
    }

  public:
    void set_display_preference( DisplayPreference s_pref ) { display_preference = s_pref; }

    void apply( Framebuffer &fb ) const;
    void new_user_byte( char the_byte, const Framebuffer &fb );
    void cull( const Framebuffer &fb );

    void reset( void );

    void set_local_frame_sent( uint64_t x ) { local_frame_sent = x; }
    void set_local_frame_acked( uint64_t x ) { local_frame_acked = x; }
    void set_local_frame_late_acked( uint64_t x ) { local_frame_late_acked = x; }

    void set_send_interval( unsigned int x ) { send_interval = x; }

    int wait_time( void ) const
    {
      return ( timing_tests_necessary() && active() )
          ? 50
          : INT_MAX;
    }

    PredictionEngine( void ) : last_byte( 0 ), parser(), overlays(), cursors(),
			       local_frame_sent( 0 ), local_frame_acked( 0 ),
			       local_frame_late_acked( 0 ),
			       prediction_epoch( 1 ), confirmed_epoch( 0 ),
			       flagging( false ),
			       srtt_trigger( false ),
			       glitch_trigger( 0 ),
			       last_quick_confirmation( 0 ),
			       send_interval( 250 ),
			       last_height( 0 ), last_width( 0 ),
			       display_preference( Adaptive )
    {
    }
  };

  class TitleEngine {
  private:
    Terminal::Framebuffer::title_type prefix;

  public:
    void apply( Framebuffer &fb ) const { fb.prefix_window_title( prefix ); }
    TitleEngine() : prefix() {}
    void set_prefix( const wstring &s );
  };

  /* the overlay manager */
  class OverlayManager {
  private:
    NotificationEngine notifications;
    PredictionEngine predictions;
    TitleEngine title;

  public:
    void apply( Framebuffer &fb );

    NotificationEngine & get_notification_engine( void ) { return notifications; }
    PredictionEngine & get_prediction_engine( void ) { return predictions; }

    void set_title_prefix( const wstring &s ) { title.set_prefix( s ); }

    OverlayManager() : notifications(), predictions(), title() {}

    int wait_time( void ) const
    {
      return std::min( notifications.wait_time(), predictions.wait_time() );
    }
  };
}

#endif
