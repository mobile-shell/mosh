#include <clocale>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "src/statesync/completeterminal.h"
#include "src/terminal/parser.h"

/*
 * Round-trip harness for the mosh frame-diff protocol: the path where a
 * Complete terminal state is serialized into HostMessage instructions
 * (Terminal::Complete::init_diff() / diff_from(), which run the frame
 * encoder in Terminal::Display::new_frame()) and reconstructed on a fresh
 * terminal by Complete::apply_string() -- exactly what the mosh server and
 * client do to stay in sync.
 *
 * The input is split with FuzzedDataProvider (scalars are consumed from the
 * back of the buffer, the host-byte payload is the rest at the front):
 *
 *   [host display instructions ...][resize?][height byte][width byte]
 *
 * A (80x24) is optionally resized to (width, height) -- bounded to keep
 * allocations sane -- and then driven with the raw host bytes. Its state is
 * serialized and replayed into B, and the display state must survive the
 * round trip. Complete::compare() is mosh's own oracle for this property
 * (cell contents and cursor position); fields the display protocol does not
 * carry (row generation counters, bell count, window title) are not
 * asserted. A mismatch abort()s: that is the state-desync bug class between
 * the mosh server's encoder and the client's emulator.
 *
 * B is not fuzzed separately: it applies the same optional Resize as A, so
 * the two framebuffers always share their dimensions. Complete::compare()
 * only forms a valid oracle between equal-sized framebuffers, and the
 * display protocol only guarantees that a diff is interpreted correctly
 * once the receiver holds the same geometry (diff_from() emits a Resize
 * instruction whenever it is diffing across a size change). Note that the
 * resize-race path -- a diff computed against a receiver that still has the
 * old size -- is deliberately not covered here, because that path currently
 * shows a genuine cursor desync (bottom-right corner after scrolling, see
 * the fuzzing run report); it can be re-enabled once that is fixed.
 */

extern "C" int LLVMFuzzerInitialize( int*, char*** )
{
  /* wcwidth(3) is locale-dependent; see hostbytes_fuzzer.cc. */
  if ( setlocale( LC_ALL, "C.UTF-8" ) == NULL ) {
    setlocale( LC_ALL, "en_US.UTF-8" );
  }
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
  FuzzedDataProvider fdp( data, size );

  const int width = fdp.ConsumeIntegralInRange<int>( 1, 200 );
  const int height = fdp.ConsumeIntegralInRange<int>( 1, 200 );
  const bool do_resize = fdp.ConsumeBool();
  const std::string host_bytes = fdp.ConsumeRemainingBytesAsString();

  Terminal::Complete a( 80, 24 );
  Terminal::Complete b( 80, 24 );

  if ( do_resize ) {
    /* Both ends track the same window size: b is not resized separately,
       see the comment at the top of the file. */
    (void)a.act( Parser::Resize( width, height ) );
    (void)b.act( Parser::Resize( width, height ) );
  }
  (void)a.act( host_bytes );

  b.apply_string( a.init_diff() );
  b.apply_string( a.diff_from( b ) );

  if ( a.compare( b ) ) {
    abort();
  }

  return 0;
}
