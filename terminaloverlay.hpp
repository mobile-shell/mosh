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
    IncorrectOrExpired,
    Inactive
  };

  class ConditionalOverlay {
  public:
    uint64_t prediction_time, expiration_frame;
    int col;
    bool active; /* represents a prediction at all */
    bool tentative; /* whether to hide when score < 0 */

    ConditionalOverlay( uint64_t s_exp, int s_col )
      : prediction_time( timestamp() ),
	expiration_frame( s_exp ), col( s_col ),
	active( false ),
	tentative( false )
    {}

    virtual ~ConditionalOverlay() {}
  };

  class ConditionalCursorMove : public ConditionalOverlay {
  public:
    int row;

    int frozen_col, frozen_row; /* after a control character */

    bool show_frozen_cursor;

    void apply( Framebuffer &fb ) const;

    Validity get_validity( const Framebuffer &fb, uint64_t current_frame ) const;

    ConditionalCursorMove( uint64_t s_exp, int s_row, int s_col )
      : ConditionalOverlay( s_exp, s_col ), row( s_row ), frozen_col( -1 ), frozen_row( -1 ),
	show_frozen_cursor( false )
    {}

    void freeze( void ) { if ( show_frozen_cursor ) { return; } frozen_col = col; frozen_row = row; show_frozen_cursor = true; }
    void thaw( void ) { show_frozen_cursor = false; }
    void reset( void ) { active = false; thaw(); }
  };

  class ConditionalOverlayCell : public ConditionalOverlay {
  public:
    Cell replacement;

    mutable uint64_t display_time;

    void apply( Framebuffer &fb, bool show_tentative, int row, bool flag ) const;
    Validity get_validity( const Framebuffer &fb, int row, uint64_t current_frame ) const;

    ConditionalOverlayCell( int s_col )
      : ConditionalOverlay( 0, s_col ),
	replacement( 0 ),
	display_time( -1 )
    {}

    void reset( void ) { active = false; display_time = -1; }
  };

  class ConditionalOverlayRow {
  public:
    int row_num;
    vector<ConditionalOverlayCell> overlay_cells;

    void apply( Framebuffer &fb, bool show_tentative, bool flag ) const;

    ConditionalOverlayRow( int s_row_num ) : row_num( s_row_num ), overlay_cells() {}
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
    Parser::UTF8Parser parser;

    list<ConditionalOverlayRow> overlays;

    ConditionalCursorMove cursor;

    int score;

    uint64_t local_frame_sent, local_frame_acked;

    ConditionalOverlayRow & get_or_make_row( int row_num, int num_cols );

    uint64_t prediction_checkpoint;

    void become_tentative( void );

    bool flagging;

  public:
    void apply( Framebuffer &fb ) const;
    void new_user_byte( char the_byte, const Framebuffer &fb );
    void cull( const Framebuffer &fb );

    void reset( void );

    void set_local_frame_sent( uint64_t x ) { local_frame_sent = x; }
    void set_local_frame_acked( uint64_t x ) { local_frame_acked = x; }

    PredictionEngine( void ) : parser(), overlays(), cursor( 0, 0, 0 ), score( 0 ),
			       local_frame_sent( 0 ), local_frame_acked( 0 ),
			       prediction_checkpoint( timestamp() ), flagging( false )
    {
      become_tentative();
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
