AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../terminal -I../protobufs $(BOOST_CPPFLAGS) $(protobuf_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) -fno-default-inline -pipe

noinst_LIBRARIES = libmoshstatesync.a

libmoshstatesync_a_SOURCES = completeterminal.cc completeterminal.h user.cc user.h
