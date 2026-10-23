AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS) $(CODE_COVERAGE_CXXFLAGS) $(FUZZING_CFLAGS)

if ENABLE_FUZZING
  noinst_PROGRAMS = terminal_parser_fuzzer terminal_fuzzer terminal_roundtrip_fuzzer
endif

terminal_parser_fuzzer_CPPFLAGS = -I$(top_srcdir)/
terminal_parser_fuzzer_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a
terminal_parser_fuzzer_SOURCES = terminal_parser_fuzzer.cc

terminal_fuzzer_CPPFLAGS = -I$(top_srcdir)/
terminal_fuzzer_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a ../statesync/libmoshstatesync.a ../protobufs/libmoshprotos.a $(TINFO_LIBS) $(protobuf_LIBS)
terminal_fuzzer_SOURCES = terminal_fuzzer.cc

terminal_roundtrip_fuzzer_CPPFLAGS = -I$(top_srcdir)/
# Complete::init_diff()/diff_from() live in libmoshstatesync.a and reference
# Terminal::Display::new_frame() from libmoshterminal.a; with single-pass
# static-archive linking the dependent archive must be scanned first.
terminal_roundtrip_fuzzer_LDADD = ../statesync/libmoshstatesync.a ../protobufs/libmoshprotos.a ../terminal/libmoshterminal.a ../util/libmoshutil.a $(TINFO_LIBS) $(protobuf_LIBS)
terminal_roundtrip_fuzzer_SOURCES = terminal_roundtrip_fuzzer.cc
