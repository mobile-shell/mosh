AM_CPPFLAGS = -I$(srcdir)/../statesync -I$(srcdir)/../terminal -I$(srcdir)/../network -I$(srcdir)/../crypto -I$(builddir)/../protobufs -I$(srcdir)/../util $(BOOST_CPPFLAGS) $(protobuf_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) -fno-default-inline -pipe
LDADD = ../crypto/libmoshcrypto.a ../network/libmoshnetwork.a ../statesync/libmoshstatesync.a ../terminal/libmoshterminal.a ../util/libmoshutil.a ../protobufs/libmoshprotos.a -lutil -lm $(protobuf_LIBS)
if COND_THIRD_LIBSTDDJB
  AM_CPPFLAGS += -I$(top_srcdir)/third/libstddjb
  LDADD += $(top_builddir)/third/libstddjb/libstddjb.a
else
  AM_CPPFLAGS += $(STDDJB_CPPFLAGS)
  LDADD += $(STDDJB_LDFLAGS)
endif

bin_PROGRAMS = mosh-client mosh-server

mosh_client_SOURCES = mosh-client.cc stmclient.cc stmclient.h terminaloverlay.cc terminaloverlay.h
mosh_server_SOURCES = mosh-server.cc
