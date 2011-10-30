#ifndef TERMINAL_OVERLAY_HPP
#define TERMINAL_OVERLAY_HPP

#include "terminalframebuffer.hpp"
#include "network.hpp"

#include <list>

namespace Overlay {
  using namespace Terminal;
  using namespace Network;
  using namespace std;

  enum Validity {
    Pending,
    Correct,
    IncorrectOrExpired
  };

  /* The individual elements of an overlay -- cursor movements and replaced cells */
  class OverlayElement {
  public:
    uint64_t prediction_time, expiration_time;
    bool flag; /* whether to bold for the user */

    virtual void apply( Framebuffer &fb ) const = 0;
    virtual Validity get_validity( const Framebuffer & ) const;

    OverlayElement( uint64_t s_expiration_time ) : prediction_time( timestamp() ),
						   expiration_time( s_expiration_time ),
						   flag( false ) {}
    virtual ~OverlayElement() {}
  };

  class OverlayCell : public OverlayElement {
  public:
    int row, col;
    Cell replacement;

    OverlayCell( uint64_t expiration_time, int s_row, int s_col, int background_color );
    void apply( Framebuffer &fb ) const;
  };

  class ConditionalOverlayCell : public OverlayCell {
  public:
    Cell original_contents;

    Validity get_validity( const Framebuffer &fb ) const;

    ConditionalOverlayCell( uint64_t expiration_time, int s_row, int s_col, int background_color,
			    Cell s_original_contents )
      : OverlayCell( expiration_time, s_row, s_col, background_color ),
	original_contents( s_original_contents )
    {}
  };

  class CursorMove : public OverlayElement {
  public:
    int new_row, new_col;

    void apply( Framebuffer &fb ) const;

    CursorMove( uint64_t expiration_time, int s_new_row, int s_new_col );
  };

  class ConditionalCursorMove : public CursorMove {
  public:
    Validity get_validity( const Framebuffer &fb ) const;

    ConditionalCursorMove( uint64_t expiration_time, int s_new_row, int s_new_col )
      : CursorMove( expiration_time, s_new_row, s_new_col )
    {}
  };

  /* the various overlays -- some predictive and some for local notifications */
  class OverlayEngine {
  protected:
    list<OverlayElement *> elements;

  public:
    virtual void apply( Framebuffer &fb ) const;
    void clear( void );

    typename list<OverlayElement *>::const_iterator begin( void ) { return elements.begin(); }
    typename list<OverlayElement *>::const_iterator end( void ) { return elements.end(); }

    OverlayEngine() : elements() {}
    virtual ~OverlayEngine();
  };

  class NotificationEngine : public OverlayEngine {
  private:
    bool needs_render;

    uint64_t last_word;
    uint64_t last_render;

    wstring message;
    uint64_t message_expiration;

  public:
    void apply( Framebuffer &fb ) const;
    void set_notification_string( const wstring s_message );
    const wstring &get_notification_string( void ) { return message; }
    void server_ping( uint64_t s_last_word );
    void render_notification( void );

    NotificationEngine();
  };

  class PredictionEngine : public OverlayEngine {
  private:
    int score;

    /* use the TCP timeout algorithm to measure appropriate echo prediction timeout */
    bool RTT_hit;
    double SRTT, RTTVAR;
    bool flagging;
    int prediction_len( void );

  public:
    void cull( const Framebuffer &fb );
    void new_user_byte( char the_byte, const Framebuffer &fb );
    void calculate_score( const Framebuffer &fb );

    PredictionEngine() : score( 0 ), RTT_hit( false ), SRTT( 1000 ), RTTVAR( 500 ), flagging( false ) {}

    int get_score( void ) { return score; }
  };

  /* the overlay manager */
  class OverlayManager {
  private:
    NotificationEngine notifications;
    PredictionEngine predictions;

  public:
    void apply( Framebuffer &fb );

    NotificationEngine & get_notification_engine( void ) { return notifications; }
    PredictionEngine & get_prediction_engine( void ) { return predictions; }

    OverlayManager() : notifications(), predictions() {}

    int wait_time( void );
  };
}

#endif
