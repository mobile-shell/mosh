AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS) $(CODE_COVERAGE_CXXFLAGS) $(FUZZING_CFLAGS)

if ENABLE_FUZZING
  noinst_PROGRAMS = terminal_parser_fuzzer terminal_fuzzer terminal_host_fuzzer statesync_fuzzer fragment_fuzzer crypto_fuzzer
endif

terminal_parser_fuzzer_CPPFLAGS = -I$(top_srcdir)/
terminal_parser_fuzzer_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a
terminal_parser_fuzzer_SOURCES = terminal_parser_fuzzer.cc

terminal_fuzzer_CPPFLAGS = -I$(top_srcdir)/
terminal_fuzzer_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a ../statesync/libmoshstatesync.a ../protobufs/libmoshprotos.a $(TINFO_LIBS) $(protobuf_LIBS)
terminal_fuzzer_SOURCES = terminal_fuzzer.cc

# Host-output escape-sequence emulator + renderer (the path a server's output
# actually drives; the older terminal_fuzzer only covers the keystroke path).
terminal_host_fuzzer_CPPFLAGS = -I$(top_srcdir)/
terminal_host_fuzzer_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a ../statesync/libmoshstatesync.a ../protobufs/libmoshprotos.a $(TINFO_LIBS) $(protobuf_LIBS)
terminal_host_fuzzer_SOURCES = terminal_host_fuzzer.cc

# Post-decryption protobuf state-sync apply path (client and server halves).
statesync_fuzzer_CPPFLAGS = -I$(top_srcdir)/ $(CRYPTO_CFLAGS)
statesync_fuzzer_LDADD = ../statesync/libmoshstatesync.a ../terminal/libmoshterminal.a ../crypto/libmoshcrypto.a ../protobufs/libmoshprotos.a ../util/libmoshutil.a $(TINFO_LIBS) $(protobuf_LIBS) $(CRYPTO_LIBS)
statesync_fuzzer_SOURCES = statesync_fuzzer.cc

# Fragment header parse + FragmentAssembly reassembly state machine.
fragment_fuzzer_CPPFLAGS = -I$(top_srcdir)/ $(CRYPTO_CFLAGS)
fragment_fuzzer_LDADD = ../network/libmoshnetwork.a ../crypto/libmoshcrypto.a ../protobufs/libmoshprotos.a ../util/libmoshutil.a $(protobuf_LIBS) $(CRYPTO_LIBS)
fragment_fuzzer_SOURCES = fragment_fuzzer.cc

# Keyless pre-auth surface: raw datagram -> Session::decrypt (AES-OCB).
crypto_fuzzer_CPPFLAGS = -I$(top_srcdir)/ $(CRYPTO_CFLAGS)
crypto_fuzzer_LDADD = ../crypto/libmoshcrypto.a ../util/libmoshutil.a $(CRYPTO_LIBS)
crypto_fuzzer_SOURCES = crypto_fuzzer.cc
