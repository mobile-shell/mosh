AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../crypto -I$(builddir)/../protobufs
AM_CXXFLAGS = -pedantic -Wno-long-long -Werror -Wall -Wextra -Weffc++ -fno-default-inline -pipe $(protobuf_CFLAGS)

noinst_LIBRARIES = libmoshnetwork.a

libmoshnetwork_a_SOURCES = network.cc network.h networktransport.cc networktransport.h transportfragment.cc transportfragment.h transportsender.cc transportsender.h transportstate.h compressor.cc compressor.h
