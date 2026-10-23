AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS) $(CODE_COVERAGE_CXXFLAGS) $(FUZZING_CFLAGS)

if ENABLE_FUZZING
  noinst_PROGRAMS = terminal_parser_fuzzer terminal_fuzzer hostmessage_fuzzer
endif

terminal_parser_fuzzer_CPPFLAGS = -I$(top_srcdir)/
terminal_parser_fuzzer_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a
terminal_parser_fuzzer_SOURCES = terminal_parser_fuzzer.cc

terminal_fuzzer_CPPFLAGS = -I$(top_srcdir)/
terminal_fuzzer_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a ../statesync/libmoshstatesync.a ../protobufs/libmoshprotos.a $(TINFO_LIBS) $(protobuf_LIBS)
terminal_fuzzer_SOURCES = terminal_fuzzer.cc

hostmessage_fuzzer_CPPFLAGS = -I$(top_srcdir)/
# Complete::apply_string() lives in libmoshstatesync.a and references
# Terminal::Display::new_frame() from libmoshterminal.a; with single-pass
# static-archive linking the dependent archive must be scanned first.
hostmessage_fuzzer_LDADD = ../statesync/libmoshstatesync.a ../protobufs/libmoshprotos.a ../terminal/libmoshterminal.a ../util/libmoshutil.a $(TINFO_LIBS) $(protobuf_LIBS)
hostmessage_fuzzer_SOURCES = hostmessage_fuzzer.cc
