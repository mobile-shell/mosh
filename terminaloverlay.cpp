#include <algorithm>
#include <wchar.h>
#include <list>

#include "terminaloverlay.hpp"

using namespace Overlay;

Validity OverlayElement::get_validity( const Framebuffer & ) const
{
  return (timestamp() < expiration_time) ? Pending : IncorrectOrExpired;
}

void OverlayCell::apply( Framebuffer &fb ) const
{
  if ( (row >= fb.ds.get_height())
       || (col >= fb.ds.get_width()) ) {
    return;
  }

  *(fb.get_mutable_cell( row, col )) = replacement;
}

Validity ConditionalOverlayCell::get_validity( const Framebuffer &fb ) const
{
  if ( (row >= fb.ds.get_height())
       || (col >= fb.ds.get_width()) ) {
    return IncorrectOrExpired;
  }

  const Cell &current = *( fb.get_cell( row, col ) );
  
  if ( (timestamp() < expiration_time) && (current == original_contents) ) {
    return Pending;
  }

  if ( current == replacement ) {
    return Correct;
  } else {
    return IncorrectOrExpired;
  }
}

void CursorMove::apply( Framebuffer &fb ) const
{
  assert( new_row < fb.ds.get_height() );
  assert( new_col < fb.ds.get_width() );
  assert( !fb.ds.origin_mode );

  fb.ds.move_row( new_row, false );
  fb.ds.move_col( new_col, false, false );
}

Validity ConditionalCursorMove::get_validity( const Framebuffer &fb ) const
{
  if ( timestamp() < expiration_time ) {
    return Pending;
  }

  if ( (fb.ds.get_cursor_col() == new_col)
       && (fb.ds.get_cursor_row() == new_row) ) {
    return Correct;
  } else {
    return IncorrectOrExpired;
  }
}

void OverlayEngine::clear( void )
{
  for_each( elements.begin(), elements.end(), []( OverlayElement *x ){ delete x; } );
  elements.clear();
}

OverlayEngine::~OverlayEngine()
{
  clear();
}

void OverlayEngine::apply( Framebuffer &fb ) const
{
  for_each( elements.begin(), elements.end(),
	    [&fb]( OverlayElement *x ) { x->apply( fb ); } );
}

void OverlayEngine::cull( const Framebuffer &fb )
{
  elements.remove_if( [fb]( OverlayElement *x ) { return IncorrectOrExpired == x->get_validity( fb ); } );
}

OverlayCell::OverlayCell( uint64_t expiration_time, int s_row, int s_col, int background_color )
  : OverlayElement( expiration_time ), row( s_row ), col( s_col ), replacement( background_color )
{}

NotificationEngine::NotificationEngine()
  : needs_render( true ),
    last_word( timestamp() ),
    last_render( 0 ),
    message(),
    message_expiration( 0 )
{}

void NotificationEngine::server_ping( uint64_t s_last_word )
{
  if ( s_last_word - last_word > 4000 ) {
    needs_render = true;
  }

  last_word = s_last_word;
}

void NotificationEngine::set_notification_string( const wstring s_message )
{
  message = s_message;
  message_expiration = timestamp() + 1100;
  needs_render = true;
}

void NotificationEngine::render_notification( void )
{
  uint64_t now = timestamp();

  if ( (now - last_render < 250) && (!needs_render) ) {
    return;
  }

  needs_render = false;
  last_render = now;

  clear();

  /* determine string to draw */
  if ( now >= message_expiration ) {
    message.clear();
  }

  bool time_expired = now - last_word > 5000;

  wchar_t tmp[ 128 ];

  if ( message.empty() && (!time_expired) ) {
    return;
  } else if ( message.empty() && time_expired ) {
    swprintf( tmp, 128, L"[stm] No contact for %.0f seconds.", (double)(now - last_word) / 1000.0 );
  } else if ( (!message.empty()) && (!time_expired) ) {
    swprintf( tmp, 128, L"[stm] %ls", message.c_str() );
  } else {
    swprintf( tmp, 128, L"[stm] %ls (No contact for %.0f seconds.)", message.c_str(),
	      (double)(now - last_word) / 1000.0 );
  }

  wstring string_to_draw( tmp );

  int overlay_col = 0;
  bool dirty = false;
  OverlayCell template_cell( now + 1100, 0 /* row */, -1 /* col */, 0 /* background_color */ );

  template_cell.replacement.renditions.inverse = true;

  OverlayCell current( template_cell );

  for ( wstring::const_iterator i = string_to_draw.begin(); i != string_to_draw.end(); i++ ) {
    wchar_t ch = *i;
    int chwidth = ch == L'\0' ? -1 : wcwidth( ch );

    switch ( chwidth ) {
    case 1: /* normal character */
    case 2: /* wide character */
      /* finish current cell */
      if ( dirty ) {
	elements.push_back( new OverlayCell( current ) );
	dirty = false;
      }
      
      /* initialize new cell */
      current = template_cell;
      current.col = overlay_col;
      current.replacement.contents.push_back( ch );
      current.replacement.width = chwidth;
      overlay_col += chwidth;
      dirty = true;
      break;

    case 0: /* combining character */
      if ( current.replacement.contents.empty() ) {
	/* string starts with combining character?? */
	/* emulate fallback rendering */
	current = template_cell;
	current.col = overlay_col;
	current.replacement.contents.push_back( 0xA0 ); /* no-break space */
	current.replacement.width = 1;
	overlay_col++;
	dirty = true;
      }

      current.replacement.contents.push_back( ch );
      break;

    case -1:
      break;
      
    default:
      assert( false );
    }
  }

  if ( dirty ) {
    elements.push_back( new OverlayCell( current ) );
  }
}

void NotificationEngine::apply( Framebuffer &fb ) const
{
  if ( elements.empty() ) {
    return;
  }

  assert( fb.ds.get_width() > 0 );
  assert( fb.ds.get_height() > 0 );

  Cell notification_bar( 0 );
  notification_bar.renditions.inverse = true;
  notification_bar.contents.push_back( 0x20 );

  for ( int i = 0; i < fb.ds.get_width(); i++ ) {
    *(fb.get_mutable_cell( 0, i )) = notification_bar;
  }

  OverlayEngine::apply( fb );
}

void OverlayManager::apply( Framebuffer &fb ) const
{
  notifications.apply( fb );
}
