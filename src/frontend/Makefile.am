AM_CPPFLAGS = -I$(top_srcdir)/ $(TINFO_CFLAGS) $(protobuf_CFLAGS) $(CRYPTO_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS) $(CODE_COVERAGE_CXXFLAGS)
AM_LDFLAGS  = $(HARDEN_LDFLAGS) $(CODE_COVERAGE_LIBS)
LDADD = ../crypto/libmoshcrypto.a ../network/libmoshnetwork.a ../statesync/libmoshstatesync.a ../terminal/libmoshterminal.a ../util/libmoshutil.a ../protobufs/libmoshprotos.a -lm $(TINFO_LIBS) $(protobuf_LIBS) $(CRYPTO_LIBS)

mosh_server_LDADD = $(LDADD)

# The prediction engine lives in a library so that src/tests can link it.
noinst_LIBRARIES = libmoshoverlay.a
libmoshoverlay_a_SOURCES = terminaloverlay.cc terminaloverlay.h

bin_PROGRAMS =

if BUILD_CLIENT
  bin_PROGRAMS += mosh-client
endif

if BUILD_SERVER
  bin_PROGRAMS += mosh-server
endif

mosh_client_SOURCES = mosh-client.cc stmclient.cc stmclient.h
mosh_client_LDADD = libmoshoverlay.a $(LDADD)
mosh_server_SOURCES = mosh-server.cc
