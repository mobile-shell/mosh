#include <algorithm>
#include <wchar.h>
#include <list>
#include <typeinfo>
#include <limits.h>

#include "terminaloverlay.hpp"

using namespace Overlay;

void ConditionalOverlayCell::apply( Framebuffer &fb, bool show_tentative, int row, bool flag ) const
{
  if ( (!active)
       || (row >= fb.ds.get_height())
       || (col >= fb.ds.get_width()) ) {
    return;
  }

  if ( tentative && (!show_tentative) ) {
    return;
  }

  /*
  fprintf( stderr, "APPLYING char %lc to (%d, %d)\n",
	   replacement.debug_contents(), row, col );
  */

  if ( !(*(fb.get_cell( row, col )) == replacement) ) {
    *(fb.get_mutable_cell( row, col )) = replacement;
    uint64_t now = timestamp();
    if ( display_time >= now ) {
      display_time = now;
    }
    if ( flag ) {
      fb.get_mutable_cell( row, col )->renditions.underlined = true;
    }
  }
}

Validity ConditionalOverlayCell::get_validity( const Framebuffer &fb, int row, uint64_t current_frame ) const
{
  if ( !active ) {
    return Inactive;
  }

  if ( (row >= fb.ds.get_height())
       || (col >= fb.ds.get_width()) ) {
    return IncorrectOrExpired;
  }

  const Cell &current = *( fb.get_cell( row, col ) );

  /* see if it hasn't been updated yet */
  if ( (current_frame < expiration_frame) ) {
    return Pending;
  }

  /* special case deletion */
  if ( current.contents.empty() && (replacement.contents.size() == 1) && (replacement.contents.front() == 0x20) ) {
    return Correct;
  }

  if ( current == replacement ) {
    return Correct;
  } else {
    return IncorrectOrExpired;
  }
}

Validity ConditionalCursorMove::get_validity( const Framebuffer &fb, uint64_t current_frame ) const
{
  if ( !active ) {
    return Inactive;
  }

  if ( (row >= fb.ds.get_height())
       || (col >= fb.ds.get_width()) ) {
    fprintf( stderr, "Crazy cursor (%d,%d)!\n", row, col );
    return IncorrectOrExpired;
  }

  if ( (fb.ds.get_cursor_col() == col)
       && (fb.ds.get_cursor_row() == row) ) {
    return Correct;
  } else if ( current_frame < expiration_frame ) {
    return Pending;
  } else {

    fprintf( stderr, "Bad cursor in %d (i thought %d vs %d).\n", (int)current_frame,
    	     col, fb.ds.get_cursor_col() );
    return IncorrectOrExpired;
  }
}

void ConditionalCursorMove::apply( Framebuffer &fb ) const
{
  if ( !active ) {
    return;
  }

  int target_row = row;
  int target_col = col;

  if ( show_frozen_cursor ) {
    target_row = frozen_row;
    target_col = frozen_col;
  }

  assert( target_row < fb.ds.get_height() );
  assert( target_col < fb.ds.get_width() );
  assert( !fb.ds.origin_mode );

  fb.ds.move_row( target_row, false );
  fb.ds.move_col( target_col, false, false );
}

NotificationEngine::NotificationEngine()
  : last_word_from_server( timestamp() ),
    message(),
    message_expiration( -1 )
{}

void NotificationEngine::apply( Framebuffer &fb ) const
{
  uint64_t now = timestamp();

  bool time_expired = need_countup( now );

  if ( message.empty() && !time_expired ) {
    return;
  }

  assert( fb.ds.get_width() > 0 );
  assert( fb.ds.get_height() > 0 );

  /* hide cursor if necessary */
  if ( fb.ds.get_cursor_row() == 0 ) {
    fb.ds.cursor_visible = false;
  }

  /* draw bar across top of screen */
  Cell notification_bar( 0 );
  notification_bar.renditions.foreground_color = 37;
  notification_bar.renditions.background_color = 44;
  notification_bar.contents.push_back( 0x20 );

  for ( int i = 0; i < fb.ds.get_width(); i++ ) {
    *(fb.get_mutable_cell( 0, i )) = notification_bar;
  }

  /* write message */
  wchar_t tmp[ 128 ];

  if ( message.empty() && (!time_expired) ) {
    return;
  } else if ( message.empty() && time_expired ) {
    swprintf( tmp, 128, L"mosh: Last contact %.0f seconds ago. [To quit: Ctrl-^ .]", (double)(now - last_word_from_server) / 1000.0 );
  } else if ( (!message.empty()) && (!time_expired) ) {
    swprintf( tmp, 128, L"mosh: %ls [To quit: Ctrl-^ .]", message.c_str() );
  } else {
    swprintf( tmp, 128, L"mosh: %ls (%.0f s without contact.) [To quit: Ctrl-^ .]", message.c_str(),
	      (double)(now - last_word_from_server) / 1000.0 );
  }

  wstring string_to_draw( tmp );

  int overlay_col = 0;

  Cell *combining_cell = fb.get_mutable_cell( 0, 0 );

  /* We unfortunately duplicate the terminal's logic for how to render a Unicode sequence into graphemes */
  for ( wstring::const_iterator i = string_to_draw.begin(); i != string_to_draw.end(); i++ ) {
    if ( overlay_col >= fb.ds.get_width() ) {
      break;
    }

    wchar_t ch = *i;
    int chwidth = ch == L'\0' ? -1 : wcwidth( ch );
    Cell *this_cell = nullptr;

    switch ( chwidth ) {
    case 1: /* normal character */
    case 2: /* wide character */
      this_cell = fb.get_mutable_cell( 0, overlay_col );
      fb.reset_cell( this_cell );
      this_cell->renditions.bold = true;
      this_cell->renditions.foreground_color = 37;
      this_cell->renditions.background_color = 44;
      
      this_cell->contents.push_back( ch );
      this_cell->width = chwidth;
      combining_cell = this_cell;

      overlay_col += chwidth;
      break;
    case 0: /* combining character */
      if ( !combining_cell ) {
	break;
      }

      if ( combining_cell->contents.size() == 0 ) {
	assert( combining_cell->width == 1 );
	combining_cell->fallback = true;
	overlay_col++;
      }

      if ( combining_cell->contents.size() < 16 ) {
	combining_cell->contents.push_back( ch );
      }
      break;
    case -1: /* unprintable character */
      break;
    default:
      assert( false );
    }
  }
}

void NotificationEngine::adjust_message( void )
{
  if ( timestamp() >= message_expiration ) {
    message.clear();
  }  
}

void OverlayManager::apply( Framebuffer &fb )
{
  predictions.cull( fb );
  predictions.apply( fb );
  notifications.adjust_message();
  notifications.apply( fb );
  title.apply( fb );
}

int OverlayManager::wait_time( void )
{
  uint64_t next_expiry = INT_MAX;

  uint64_t message_delay = notifications.get_message_expiration() - timestamp();

  if ( message_delay < next_expiry ) {
    next_expiry = message_delay;
  }

  if ( notifications.need_countup( timestamp() ) && ( next_expiry > 1000 ) ) {
    next_expiry = 1000;
  }

  return next_expiry;
}

void TitleEngine::set_prefix( const wstring s )
{
  prefix = deque<wchar_t>( s.begin(), s.end() );
}

void ConditionalOverlayRow::apply( Framebuffer &fb, bool show_tentative, bool flag ) const
{
  for_each( overlay_cells.begin(), overlay_cells.end(), [&]( const ConditionalOverlayCell &x ) { x.apply( fb, show_tentative, row_num, flag ); } );
}

void PredictionEngine::apply( Framebuffer &fb ) const
{
  if ( (score > 0) || cursor.show_frozen_cursor ) {
    cursor.apply( fb );
  }
  for_each( overlays.begin(), overlays.end(), [&]( const ConditionalOverlayRow &x ){ x.apply( fb, score > 0, flagging ); } );
}

void PredictionEngine::reset( void )
{
  overlays.clear();
  cursor.reset();
  become_tentative();
}

void PredictionEngine::cull( const Framebuffer &fb )
{
  if ( score > 0 ) {
    cursor.thaw();
  }

  uint64_t now = timestamp();

  /* don't increment score just for correct cursor position */
  switch ( cursor.get_validity( fb, local_frame_acked) ) {
  case IncorrectOrExpired:
    cursor.reset();
    become_tentative();
    return;
    break;
  default:
    break;
  }

  uint64_t max_delay = 0;

  auto i = overlays.begin();
  while ( i != overlays.end() ) {
    auto inext = i;
    inext++;
    if ( (i->row_num < 0) || (i->row_num >= fb.ds.get_height()) ) {
      overlays.erase( i );
      i = inext;
      continue;
    }

    for ( auto j = i->overlay_cells.begin(); j != i->overlay_cells.end(); j++ ) {
      switch ( j->get_validity( fb, i->row_num, local_frame_acked ) ) {
      case IncorrectOrExpired:
	if ( j->tentative ) {
	  fprintf( stderr, "Bad tentative prediction in row %d, col %d (thought %lc, was %lc)\n",
		   i->row_num, j->col,
		   j->replacement.debug_contents(), fb.get_cell( i->row_num, j->col )->debug_contents() );
	  j->reset();
	  become_tentative();
	  if ( j->display_time != uint64_t(-1) ) {
	    fprintf( stderr, "TIMING %ld - %ld (TENT)\n", time(NULL), now - j->display_time );
	  }
	} else {
	  fprintf( stderr, "[%d=>%d] (score=%d) Killing prediction in row %d, col %d (thought %lc, was %lc)\n",
		   (int)local_frame_acked, (int)j->expiration_frame,
		   score,
		   i->row_num, j->col,
		   j->replacement.debug_contents(), fb.get_cell( i->row_num, j->col )->debug_contents() );
	  reset();
	  if ( j->display_time != uint64_t(-1) ) {
	    fprintf( stderr, "TIMING %ld - %ld\n", time(NULL), now - j->display_time );
	  }
	  return;
	}
	break;
      case Correct:
	if ( j->display_time != uint64_t(-1) ) {
	  fprintf( stderr, "TIMING %ld + %ld\n", now, now - j->display_time );
	}

	j->reset();
	if ( j->prediction_time > prediction_checkpoint ) {
	  score++;
	}
	break;
      case Pending:
	max_delay = max( max_delay, now - j->prediction_time );
	break;
      default:
	break;
      }
    }

    i = inext;
  }

  if ( max_delay > 100 ) {
    flagging = true;
  } else if ( max_delay < 50 ) {
    flagging = false;
  }
}

ConditionalOverlayRow & PredictionEngine::get_or_make_row( int row_num, int num_cols )
{
  auto it = find_if( overlays.begin(), overlays.end(),
		     [&]( const ConditionalOverlayRow &x ) { return x.row_num == row_num; } );

  if ( it != overlays.end() ) {
    return *it;
  } else {
    /* make row */
    ConditionalOverlayRow r( row_num );
    r.overlay_cells.reserve( num_cols );
    for ( int i = 0; i < num_cols; i++ ) {
      r.overlay_cells.push_back( ConditionalOverlayCell( i ) );
      assert( r.overlay_cells[ i ].col == i );
    }
    overlays.push_back( r );
    return overlays.back();
  }
}

void PredictionEngine::new_user_byte( char the_byte, const Framebuffer &fb )
{
  list<Parser::Action *> actions( parser.input( the_byte ) );

  uint64_t now = timestamp();

  for ( auto it = actions.begin(); it != actions.end(); it++ ) {
    Parser::Action *act = *it;

    if ( typeid( *act ) == typeid( Parser::Print ) ) {
      /* make new prediction */

      wchar_t ch = act->ch;
      /* XXX handle wide characters */

      if ( !cursor.active ) {
	/* initialize new cursor prediction */
	cursor.row = fb.ds.get_cursor_row();
	cursor.col = fb.ds.get_cursor_col();
	cursor.active = true;
	cursor.prediction_time = now;
	cursor.expiration_frame = local_frame_sent + 1;
      }
      
      if ( ch == 0x7f ) { /* backspace */
	//	fprintf( stderr, "Backspace.\n" );
	if ( cursor.col > 0 ) {
	  cursor.col--;

	  cursor.prediction_time = now;
	  cursor.expiration_frame = local_frame_sent + 1;

	  ConditionalOverlayCell &cell = get_or_make_row( cursor.row, fb.ds.get_width() ).overlay_cells[ cursor.col ];
	  cell.active = true;
	  cell.tentative = (score <= 0);
	  cell.prediction_time = now;
	  cell.expiration_frame = local_frame_sent + 1;
	  cell.replacement.renditions = fb.ds.get_renditions();
	  cell.replacement.contents.clear();
	  cell.replacement.contents.push_back( 0x20 );
	  cell.display_time = -1;
	}
      } else if ( (ch < 0x20) || (ch > 0x7E) ) {
	/* unknown print */
	become_tentative();
      } else {
	/* don't attempt to change existing blank or space cells if user has typed space */
	const Cell *existing_cell = fb.get_cell( cursor.row, cursor.col );
	if ( ( existing_cell->contents.empty() || ((existing_cell->contents.size() == 1)
						   && ( (existing_cell->contents.front() == 0x20)
							|| (existing_cell->contents.front() == 0xA0) )))
	     && ( ch == 0x20 ) ) {
	  /* do nothing */
	} else {
	  assert( cursor.row >= 0 );
	  assert( cursor.col >= 0 );
	  assert( cursor.row < fb.ds.get_height() );
	  assert( cursor.col < fb.ds.get_width() );

	  if ( cursor.col + 1 >= fb.ds.get_width() ) {
	    /* prediction in the last column is tricky */
	    /* e.g., emacs will show wrap character, shell will just put the character there */
	    become_tentative();
	    cursor.freeze();
	  }

	  ConditionalOverlayCell &cell = get_or_make_row( cursor.row, fb.ds.get_width() ).overlay_cells[ cursor.col ];
	  cell.active = true;
	  cell.tentative = (score <= 0);
	  cell.prediction_time = now;
	  cell.expiration_frame = local_frame_sent + 1;
	  cell.replacement.renditions = fb.ds.get_renditions();
	  cell.replacement.contents.clear();
	  cell.replacement.contents.push_back( ch );
	  cell.display_time = -1;

	  /*
	  fprintf( stderr, "[%d=>%d] Predicting %lc in row %d, col %d\n",
		   (int)local_frame_acked, (int)cell.expiration_frame,
		   ch, cursor.row, cursor.col );
	  */
	}

	cursor.prediction_time = now;
	cursor.expiration_frame = local_frame_sent + 1;
	cursor.col++;

	/* do we need to wrap? */
	if ( cursor.col >= fb.ds.get_width() ) {
	  become_tentative();
	  cursor.col--;
	  cursor.freeze();
	  cursor.col = 0;
	  if ( cursor.row == fb.ds.get_height() - 1 ) {
	    for ( auto i = overlays.begin(); i != overlays.end(); i++ ) {
	      i->row_num--;
	      for ( auto j = i->overlay_cells.begin(); j != i->overlay_cells.end(); j++ ) {
		if ( j->active ) {
		  //		  j->tentative = (score <= 0);
		  j->prediction_time = now;
		  j->expiration_frame = local_frame_sent + 1;
		}
	      }
	    }
	  } else {
	    cursor.row++;
	  }
	}
      }
    } else if ( typeid( *act ) == typeid( Parser::Execute ) ) {
      become_tentative();
      cursor.freeze();
    }

    delete act;
  }
}

void PredictionEngine::become_tentative( void )
{
  prediction_checkpoint = timestamp();
  score = 0;
}
