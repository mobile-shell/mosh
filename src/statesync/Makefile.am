AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../terminal -I../protobufs $(protobuf_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS)

noinst_LIBRARIES = libmoshstatesync.a

libmoshstatesync_a_SOURCES = completeterminal.cc completeterminal.h user.cc user.h
