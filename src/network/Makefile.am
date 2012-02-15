AM_CPPFLAGS = -I$(srcdir)/../util -I$(srcdir)/../crypto -I$(builddir)/../protobufs
AM_CXXFLAGS = --std=c++0x -pedantic -Wall -Wextra -Weffc++ -fno-default-inline -pipe

noinst_LIBRARIES = libmoshnetwork.a

libmoshnetwork_a_SOURCES = network.cc network.h networktransport.cc networktransport.h transportfragment.cc transportfragment.h transportsender.cc transportsender.h transportstate.h
