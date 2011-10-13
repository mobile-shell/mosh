#include <algorithm>
#include <wchar.h>
#include <list>
#include <typeinfo>

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
  fb.get_mutable_cell( row, col )->renditions.bold = true; /* XXX */
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
  auto i = elements.begin();
  while ( i != elements.end() ) {
    if ( (*i)->get_validity( fb ) != Pending ) {
      delete (*i);
      i = elements.erase( i );
    } else {
      i++;
    }
  }
}

OverlayCell::OverlayCell( uint64_t expiration_time, int s_row, int s_col, int background_color )
  : OverlayElement( expiration_time ), row( s_row ), col( s_col ), replacement( background_color )
{}

CursorMove::CursorMove( uint64_t expiration_time, int s_new_row, int s_new_col )
  : OverlayElement( expiration_time ), new_row( s_new_row ), new_col( s_new_col )
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
    swprintf( tmp, 128, L"[stm] No contact for %.0f seconds. [To quit: Ctrl-^ .]", (double)(now - last_word) / 1000.0 );
  } else if ( (!message.empty()) && (!time_expired) ) {
    swprintf( tmp, 128, L"[stm] %ls", message.c_str() );
  } else {
    swprintf( tmp, 128, L"[stm] %ls [To quit: Ctrl-^ .] (No contact for %.0f seconds.)", message.c_str(),
	      (double)(now - last_word) / 1000.0 );
  }

  wstring string_to_draw( tmp );

  int overlay_col = 0;
  bool dirty = false;
  OverlayCell template_cell( now + 1100, 0 /* row */, -1 /* col */, 0 /* background_color */ );

  template_cell.replacement.renditions.bold = true;
  template_cell.replacement.renditions.foreground_color = 37;
  template_cell.replacement.renditions.background_color = 44;

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


  /* draw bar across top of screen */
  Cell notification_bar( 0 );
  notification_bar.renditions.foreground_color = 37;
  notification_bar.renditions.background_color = 44;
  notification_bar.contents.push_back( 0x20 );

  for ( int i = 0; i < fb.ds.get_width(); i++ ) {
    *(fb.get_mutable_cell( 0, i )) = notification_bar;
  }

  /* hide cursor if necessary */
  if ( fb.ds.get_cursor_row() == 0 ) {
    fb.ds.cursor_visible = false;
  }

  OverlayEngine::apply( fb );
}

void OverlayManager::apply( Framebuffer &fb )
{
  calculate_score( fb );
  predictions.cull( fb );

  if ( prediction_score > 3 ) {
    predictions.apply( fb );
  } else if ( prediction_score == -1 ) {
    predictions.clear();
    prediction_score = 0;
  }

  notifications.apply( fb );
}

void OverlayManager::calculate_score( const Framebuffer &fb )
{
  for ( auto i = predictions.begin(); i != predictions.end(); i++ ) {
    switch( (*i)->get_validity( fb ) ) {
    case Pending:
      break;
    case Correct:
      prediction_score++;
      break;
    case IncorrectOrExpired:
      prediction_score = -1;
      return;
    }
  }
}

void PredictionEngine::new_user_byte( char the_byte, const Framebuffer &fb )
{
  uint64_t now = timestamp();
  int prediction_len = 1000; /* XXX */

  if ( elements.empty() ) {
    /* starting from scratch */
    
    elements.push_front( new ConditionalCursorMove( now + prediction_len,
						    fb.ds.get_cursor_row(),
						    fb.ds.get_cursor_col() ) );
  }

  assert( typeid( ConditionalCursorMove ) == typeid( *elements.front() ) );
  ConditionalCursorMove *ccm = static_cast<ConditionalCursorMove *>( elements.front() );

  if ( (ccm->new_row >= fb.ds.get_height()) || (ccm->new_col >= fb.ds.get_width()) ) {
    return;
  }

  if ( (the_byte >= 0x20) && (the_byte <= 0x7E) ) {
    /* XXX need to kill existing prediction if present */

    const Cell *existing_cell = fb.get_cell( ccm->new_row, ccm->new_col );

    ConditionalOverlayCell *coc = new ConditionalOverlayCell( now + prediction_len,
							      ccm->new_row, ccm->new_col,
							      existing_cell->renditions.background_color,
							      *existing_cell );
  
    coc->replacement = *existing_cell;
    coc->replacement.contents.clear();
    coc->replacement.contents.push_back( the_byte );

    ccm->new_col++;
    ccm->expiration_time = now + prediction_len;

    elements.push_back( coc );
  } else {
    clear();
  }
}
