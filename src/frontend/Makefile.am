AM_CPPFLAGS = -I$(srcdir)/../statesync -I$(srcdir)/../terminal -I$(srcdir)/../network -I$(srcdir)/../crypto -I$(builddir)/../protobufs -I$(srcdir)/../util
AM_CXXFLAGS = --std=c++0x -pedantic -Wall -Wextra -Weffc++ -fno-default-inline -pipe
LIBS = $(protobuf_lite_LIBS)
LDADD = ../crypto/libmoshcrypto.a ../network/libmoshnetwork.a ../statesync/libmoshstatesync.a ../terminal/libmoshterminal.a ../util/libmoshutil.a ../protobufs/libmoshprotos.a -lutil -lrt -lm -lutempter

bin_PROGRAMS = mosh-client mosh-server

mosh_client_SOURCES = mosh-client.cc stmclient.cc stmclient.h terminaloverlay.cc terminaloverlay.h
mosh_server_SOURCES = mosh-server.cc
