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
    uint64_t expiration_time;

    virtual void apply( Framebuffer &fb ) const = 0;
    virtual Validity get_validity( const Framebuffer & ) const;

    OverlayElement( uint64_t s_expiration_time ) : expiration_time( s_expiration_time ) {}
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
  };

  class CursorMove : public OverlayElement {
  public:
    int new_row, new_col;

    void apply( Framebuffer &fb ) const;
  };

  class ConditionalCursorMove : public CursorMove {
  public:
    Validity get_validity( const Framebuffer &fb ) const;
  };

  /* the various overlays -- some predictive and some for local notifications */
  class OverlayEngine {
  protected:
    list<OverlayElement *> elements;

  public:
    void cull( const Framebuffer &fb );
    virtual void apply( Framebuffer &fb ) const;
    void clear( void );

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
    void server_ping( uint64_t s_last_word );
    void render_notification( void );

    NotificationEngine();
  };

  /* the overlay manager */
  class OverlayManager {
  private:
    NotificationEngine notifications;

  public:
    void apply( Framebuffer &fb ) const;

    NotificationEngine & get_notification_engine( void ) { return notifications; }

    OverlayManager() : notifications() {} 
  };
}

#endif
