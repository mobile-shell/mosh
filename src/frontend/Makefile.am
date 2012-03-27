AM_CPPFLAGS = -I$(srcdir)/../statesync -I$(srcdir)/../terminal -I$(srcdir)/../network -I$(srcdir)/../crypto -I../protobufs -I$(srcdir)/../util $(BOOST_CPPFLAGS) $(protobuf_CFLAGS) $(poll_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) -fno-default-inline -pipe
LDADD = ../crypto/libmoshcrypto.a ../network/libmoshnetwork.a ../statesync/libmoshstatesync.a ../terminal/libmoshterminal.a ../util/libmoshutil.a ../protobufs/libmoshprotos.a $(poll_LIBS) -lm $(protobuf_LIBS)
if COND_THIRD_LIBSTDDJB
  AM_CPPFLAGS += -I$(top_srcdir)/third/libstddjb
  LDADD += $(top_builddir)/third/libstddjb/libstddjb.a
else
  AM_CPPFLAGS += $(STDDJB_CPPFLAGS)
  LDADD += $(STDDJB_LDFLAGS)
endif

mosh_server_LDADD = $(LDADD) -lutil

bin_PROGRAMS =

if BUILD_CLIENT
  bin_PROGRAMS += mosh-client
endif

if BUILD_SERVER
  bin_PROGRAMS += mosh-server
endif

mosh_client_SOURCES = mosh-client.cc stmclient.cc stmclient.h terminaloverlay.cc terminaloverlay.h
mosh_server_SOURCES = mosh-server.cc
