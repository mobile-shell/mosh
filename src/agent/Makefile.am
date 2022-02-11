AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../crypto -I$(srcdir)/../network -I../protobufs $(protobuf_CFLAGS) $(TINFO_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS)

noinst_LIBRARIES = libmoshagent.a

libmoshagent_a_SOURCES = agent.h agent.cc

