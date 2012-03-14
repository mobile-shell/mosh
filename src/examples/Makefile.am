AM_CPPFLAGS = $(protobuf_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS)
AM_LDFLAGS  = $(HARDEN_LDFLAGS)

if BUILD_EXAMPLES
  noinst_PROGRAMS = encrypt decrypt ntester parse termemu benchmark
endif

encrypt_SOURCES = encrypt.cc
encrypt_CPPFLAGS = -I$(srcdir)/../crypto
encrypt_LDADD = ../crypto/libmoshcrypto.a

decrypt_SOURCES = decrypt.cc
decrypt_CPPFLAGS = -I$(srcdir)/../crypto
decrypt_LDADD = ../crypto/libmoshcrypto.a

parse_SOURCES = parse.cc
parse_CPPFLAGS = -I$(srcdir)/../terminal -I$(srcdir)/../util $(poll_CFLAGS)
parse_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a -lutil $(poll_LIBS)

termemu_SOURCES = termemu.cc
termemu_CPPFLAGS = -I$(srcdir)/../terminal -I$(srcdir)/../util -I$(srcdir)/../statesync -I../protobufs $(poll_CFLAGS)
termemu_LDADD = ../terminal/libmoshterminal.a ../util/libmoshutil.a ../statesync/libmoshstatesync.a ../protobufs/libmoshprotos.a -lutil $(TINFO_LIBS) $(protobuf_LIBS) $(poll_LIBS)
if COND_THIRD_LIBSTDDJB
  termemu_CPPFLAGS += -I$(top_srcdir)/third/libstddjb
  termemu_LDADD += $(top_builddir)/third/libstddjb/libstddjb.a
else
  termemu_CPPFLAGS += $(STDDJB_CPPFLAGS)
  termemu_LDADD += $(STDDJB_LDFLAGS)
endif

ntester_SOURCES = ntester.cc
ntester_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../statesync -I$(srcdir)/../terminal -I$(srcdir)/../network -I$(srcdir)/../crypto -I../protobufs $(poll_CFLAGS)
ntester_LDADD = ../statesync/libmoshstatesync.a ../terminal/libmoshterminal.a ../network/libmoshnetwork.a ../crypto/libmoshcrypto.a ../protobufs/libmoshprotos.a $(poll_LIBS) -lutil -lm $(protobuf_LIBS)

benchmark_SOURCES = benchmark.cc
benchmark_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../statesync -I$(srcdir)/../terminal -I../protobufs -I$(srcdir)/../frontend -I$(srcdir)/../crypto -I$(srcdir)/../network $(poll_CFLAGS)
benchmark_LDADD = ../frontend/terminaloverlay.o ../statesync/libmoshstatesync.a ../terminal/libmoshterminal.a ../protobufs/libmoshprotos.a ../network/libmoshnetwork.a ../crypto/libmoshcrypto.a ../util/libmoshutil.a $(poll_LIBS) -lutil -lm $(TINFO_LIBS) $(protobuf_LIBS)
if COND_THIRD_LIBSTDDJB
  benchmark_CPPFLAGS += -I$(top_srcdir)/third/libstddjb
  benchmark_LDADD += $(top_builddir)/third/libstddjb/libstddjb.a
else
  benchmark_CPPFLAGS += $(STDDJB_CPPFLAGS)
  benchmark_LDADD += $(STDDJB_LDFLAGS)
endif
