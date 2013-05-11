AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../crypto -I$(srcdir)/../network -I../protobufs $(TINFO_CFLAGS) $(protobuf_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS) $(CODE_COVERAGE_CXXFLAGS)
AM_LDFLAGS  = $(HARDEN_LDFLAGS) $(CODE_COVERAGE_LIBS)

noinst_LIBRARIES = libmoshagent.a

libmoshagent_a_SOURCES = agent.cc agent.h
