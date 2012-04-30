# This must be set in Makefile.am for dependency tracking to work.
if COND_THIRD_LIBSTDDJB
  STDDJB_LDFLAGS += $(top_builddir)/third/libstddjb/libstddjb.a
  STDDJB_CPPFLAGS += -I$(top_srcdir)/third/libstddjb
endif
if COND_THIRD_POLL
  working_poll_LIBS = $(top_builddir)/third/poll/libpoll.a
  working_poll_CFLAGS = -I$(top_srcdir)/third/poll
else
  working_poll_LIBS = $(poll_LIBS)
  working_poll_CFLAGS = $(poll_CFLAGS)
endif

AM_CPPFLAGS = -I$(srcdir)/../statesync -I$(srcdir)/../terminal -I$(srcdir)/../network -I$(srcdir)/../crypto -I../protobufs -I$(srcdir)/../util $(STDDJB_CPPFLAGS) $(TINFO_CFLAGS) $(protobuf_CFLAGS) $(working_poll_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS)
AM_LDFLAGS  = $(HARDEN_LDFLAGS)
LDADD = ../crypto/libmoshcrypto.a ../network/libmoshnetwork.a ../statesync/libmoshstatesync.a ../terminal/libmoshterminal.a ../util/libmoshutil.a ../protobufs/libmoshprotos.a $(STDDJB_LDFLAGS) $(working_poll_LIBS) -lm $(TINFO_LIBS) $(protobuf_LIBS)

mosh_server_LDADD = $(LDADD) $(LIBUTIL)

bin_PROGRAMS =

if BUILD_CLIENT
  bin_PROGRAMS += mosh-client
endif

if BUILD_SERVER
  bin_PROGRAMS += mosh-server
endif

mosh_client_SOURCES = mosh-client.cc stmclient.cc stmclient.h terminaloverlay.cc terminaloverlay.h
mosh_server_SOURCES = mosh-server.cc
