#ifndef TERMINAL_OVERLAY_HPP
#define TERMINAL_OVERLAY_HPP

#include "terminalframebuffer.hpp"
#include "network.hpp"
#include "parser.hpp"

#include <vector>

namespace Overlay {
  using namespace Terminal;
  using namespace Network;
  using namespace std;

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

    ConditionalOverlay( uint64_t s_exp, int s_col, uint64_t s_tentative )
      : expiration_frame( s_exp ), expiration_time( -1 ), col( s_col ),
	active( false ),
	tentative_until_epoch( s_tentative )
    {}

    virtual ~ConditionalOverlay() {}

    bool tentative( uint64_t confirmed_epoch ) const { return tentative_until_epoch > confirmed_epoch; }
    void reset( void ) { expiration_frame = expiration_time = tentative_until_epoch = -1; active = false; }
    bool start_clock( uint64_t local_frame_acked, uint64_t now );
    void expire( uint64_t s_exp ) { expiration_frame = s_exp; expiration_time = uint64_t(-1); }
  };

  class ConditionalCursorMove : public ConditionalOverlay {
  public:
    int row;

    void apply( Framebuffer &fb, uint64_t confirmed_epoch ) const;

    Validity get_validity( const Framebuffer &fb, uint64_t current_frame, uint64_t now ) const;

    ConditionalCursorMove( uint64_t s_exp, int s_row, int s_col, uint64_t s_tentative )
      : ConditionalOverlay( s_exp, s_col, s_tentative ), row( s_row )
    {}
  };

  class ConditionalOverlayCell : public ConditionalOverlay {
  public:
    Cell replacement;
    bool unknown;

    mutable uint64_t display_time;

    vector<Cell> original_contents; /* we don't give credit for correct predictions
				       that match the original contents */

    void apply( Framebuffer &fb, uint64_t confirmed_epoch, int row, bool flag ) const;
    Validity get_validity( const Framebuffer &fb, int row, uint64_t current_frame, uint64_t now ) const;

    ConditionalOverlayCell( uint64_t s_exp, int s_col, uint64_t s_tentative )
      : ConditionalOverlay( s_exp, s_col, s_tentative ),
	replacement( 0 ),
	unknown( false ),
	display_time( uint64_t(-1) ),
	original_contents()
    {}

    void reset( void ) { unknown = false; display_time = uint64_t(-1); original_contents.clear(); ConditionalOverlay::reset(); }
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
    int first_col;

    vector<ConditionalOverlayCell> overlay_cells;

    void apply( Framebuffer &fb, uint64_t confirmed_epoch, bool flag ) const;

    ConditionalOverlayRow( int s_row_num ) : row_num( s_row_num ), first_col( INT_MAX ), overlay_cells() {}
  };

  /* the various overlays */
  class NotificationEngine {
  private:
    uint64_t last_word_from_server;
    wstring message;
    uint64_t message_expiration;

  public:
    bool need_countup( uint64_t ts ) const { return ts - last_word_from_server > 4500; }
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
    char last_byte;
    Parser::UTF8Parser parser;

    list<ConditionalOverlayRow> overlays;

    list<ConditionalCursorMove> cursors;

    uint64_t local_frame_sent, local_frame_acked;

    ConditionalOverlayRow & get_or_make_row( int row_num, int num_cols );

    uint64_t prediction_epoch;
    uint64_t confirmed_epoch;

    void become_tentative( void );

    void newline_carriage_return( const Framebuffer &fb );

    int flagging;

    ConditionalCursorMove & cursor( void ) { assert( !cursors.empty() ); return cursors.back(); }

    void kill_epoch( uint64_t epoch, const Framebuffer &fb );

    void init_cursor( const Framebuffer &fb );

    uint64_t last_scheduled_timeout;

  public:
    void apply( Framebuffer &fb ) const;
    void new_user_byte( char the_byte, const Framebuffer &fb );
    void cull( const Framebuffer &fb );

    void reset( void );

    bool active( void ) { return timestamp() <= last_scheduled_timeout; }

    void set_local_frame_sent( uint64_t x ) { local_frame_sent = x; }
    void set_local_frame_acked( uint64_t x ) { local_frame_acked = x; }

    PredictionEngine( void ) : last_byte( 0 ), parser(), overlays(), cursors(),
			       local_frame_sent( 0 ), local_frame_acked( 0 ),
			       prediction_epoch( 1 ), confirmed_epoch( 0 ),
			       flagging( 0 ), last_scheduled_timeout( 0 )
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
