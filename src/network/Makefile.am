AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../crypto -I../protobufs $(protobuf_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS)

noinst_LIBRARIES = libmoshnetwork.a

libmoshnetwork_a_SOURCES = network.cc network.h networktransport.cc networktransport.h transportfragment.cc transportfragment.h transportsender.cc transportsender.h transportstate.h compressor.cc compressor.h outofband.h outofband.cc
