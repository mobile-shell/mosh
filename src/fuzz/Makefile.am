AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS) $(CODE_COVERAGE_CXXFLAGS) $(FUZZING_CFLAGS)

if ENABLE_FUZZING
  noinst_PROGRAMS = terminal_parser_fuzzer terminal_fuzzer
endif

terminal_parser_fuzzer_CPPFLAGS = -I$(srcdir)/../terminal -I$(srcdir)/../util
terminal_parser_fuzzer_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a
terminal_parser_fuzzer_SOURCES = terminal_parser_fuzzer.cc

terminal_fuzzer_CPPFLAGS = -I$(srcdir)/../terminal -I$(srcdir)/../util -I$(srcdir)/../statesync -I../protobufs
terminal_fuzzer_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a ../statesync/libmoshstatesync.a ../protobufs/libmoshprotos.a $(TINFO_LIBS) $(protobuf_LIBS)
terminal_fuzzer_SOURCES = terminal_fuzzer.cc
