AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../crypto -I$(builddir)/../protobufs
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) -fno-default-inline -pipe

noinst_LIBRARIES = libmoshnetwork.a

libmoshnetwork_a_SOURCES = network.cc network.h networktransport.cc networktransport.h transportfragment.cc transportfragment.h transportsender.cc transportsender.h transportstate.h compressor.cc compressor.h
