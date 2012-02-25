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
*/

#ifndef TERMINAL_OVERLAY_HPP
#define TERMINAL_OVERLAY_HPP

#include "terminalframebuffer.h"
#include "network.h"
#include "parser.h"

#include <vector>

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
    uint64_t expiration_time; /* after frame is hit */
    int col;
    bool active; /* represents a prediction at all */
    uint64_t tentative_until_epoch; /* when to show */
    uint64_t prediction_time; /* used to find long-pending predictions */

    ConditionalOverlay( uint64_t s_exp, int s_col, uint64_t s_tentative )
      : expiration_frame( s_exp ), expiration_time( uint64_t( -1 ) ), col( s_col ),
	active( false ),
	tentative_until_epoch( s_tentative ), prediction_time( uint64_t( -1 ) )
    {}

    virtual ~ConditionalOverlay() {}

    bool tentative( uint64_t confirmed_epoch ) const { return tentative_until_epoch > confirmed_epoch; }
    void reset( void ) { expiration_frame = expiration_time = tentative_until_epoch = -1; active = false; }
    bool start_clock( uint64_t local_frame_acked, uint64_t now, unsigned int send_interval );
    void expire( uint64_t s_exp, uint64_t now )
    {
      expiration_frame = s_exp; expiration_time = uint64_t(-1); prediction_time = now;
    }
  };

  class ConditionalCursorMove : public ConditionalOverlay {
  public:
    int row;

    void apply( Framebuffer &fb, uint64_t confirmed_epoch ) const;

    Validity get_validity( const Framebuffer &fb, uint64_t sent_frame, uint64_t early_ack, uint64_t late_ack, uint64_t now ) const;

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
    Validity get_validity( const Framebuffer &fb, int row, uint64_t sent_frame, uint64_t early_ack, uint64_t late_ack, uint64_t now ) const;

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

      vector<Cell> new_orig( original_contents );
      new_orig.push_back( replacement );
      reset();
      original_contents = new_orig;
    }
  };

  class ConditionalOverlayRow {
  public:
    int row_num;

    vector<ConditionalOverlayCell> overlay_cells;

    void apply( Framebuffer &fb, uint64_t confirmed_epoch, bool flag ) const;

    ConditionalOverlayRow( int s_row_num ) : row_num( s_row_num ), overlay_cells() {}
  };

  /* the various overlays */
  class NotificationEngine {
  private:
    uint64_t last_word_from_server;
    wstring message;
    uint64_t message_expiration;

  public:
    bool need_countup( uint64_t ts ) const { return ts - last_word_from_server > 6500; }
    void adjust_message( void );
    void apply( Framebuffer &fb ) const;
    void set_notification_string( const wstring s_message ) { message = s_message; message_expiration = timestamp() + 1000; }
    const wstring &get_notification_string( void ) const { return message; }
    void server_heard( uint64_t s_last_word ) { last_word_from_server = s_last_word; }
    uint64_t get_message_expiration( void ) const { return message_expiration; }

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

    char last_byte;
    Parser::UTF8Parser parser;

    list<ConditionalOverlayRow> overlays;

    list<ConditionalCursorMove> cursors;

    uint64_t local_frame_sent, local_frame_acked, local_frame_late_acked;

    ConditionalOverlayRow & get_or_make_row( int row_num, int num_cols );

    uint64_t prediction_epoch;
    uint64_t confirmed_epoch;

    void become_tentative( void );

    void newline_carriage_return( const Framebuffer &fb );

    bool flagging; /* whether we are underlining predictions */
    bool srtt_trigger; /* show predictions because of slow round trip time */
    int glitch_trigger; /* show predictions temporarily because of long-pending prediction */
    uint64_t last_quick_confirmation;

    ConditionalCursorMove & cursor( void ) { assert( !cursors.empty() ); return cursors.back(); }

    void kill_epoch( uint64_t epoch, const Framebuffer &fb );

    void init_cursor( const Framebuffer &fb );

    uint64_t last_scheduled_timeout;

    unsigned int send_interval;

  public:
    enum DisplayPreference {
      Always,
      Never,
      Adaptive
    };

  private:
    DisplayPreference display_preference;

  public:
    void set_display_preference( DisplayPreference s_pref ) { display_preference = s_pref; }

    void apply( Framebuffer &fb ) const;
    void new_user_byte( char the_byte, const Framebuffer &fb );
    void cull( const Framebuffer &fb );

    void reset( void );

    bool active( void ) { return timestamp() <= last_scheduled_timeout; }

    void set_local_frame_sent( uint64_t x ) { local_frame_sent = x; }
    void set_local_frame_acked( uint64_t x ) { local_frame_acked = x; }
    void set_local_frame_late_acked( uint64_t x ) { local_frame_late_acked = x; }

    void set_send_interval( unsigned int x ) { send_interval = x; }

    PredictionEngine( void ) : last_byte( 0 ), parser(), overlays(), cursors(),
			       local_frame_sent( 0 ), local_frame_acked( 0 ),
			       local_frame_late_acked( 0 ),
			       prediction_epoch( 1 ), confirmed_epoch( 0 ),
			       flagging( false ),
			       srtt_trigger( false ),
			       glitch_trigger( 0 ),
			       last_quick_confirmation( 0 ),
			       last_scheduled_timeout( 0 ),
			       send_interval( 250 ),
			       display_preference( Adaptive )
    {
    }
  };

  class TitleEngine {
  private:
    deque<wchar_t> prefix;

  public:
    void apply( Framebuffer &fb ) const { fb.prefix_window_title( prefix ); }
    void set_prefix( const wstring s );
    TitleEngine() : prefix() {}
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

    void set_title_prefix( const wstring s ) { title.set_prefix( s ); }

    OverlayManager() : notifications(), predictions(), title() {}

    int wait_time( void );
  };
}

#endif
