AM_CPPFLAGS = -I$(top_builddir)/protobufs/ -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -D_BSD_SOURCE
AM_CXXFLAGS = -g -O2 --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-default-inline -pipe
LIBS = `pkg-config --libs protobuf-lite`
LDADD = -lutil -lcrypto -lrt -lm $(top_builddir)/protobufs/libmoshprotos.a

bin_PROGRAMS = mosh-client mosh-server

source = parser.cc parserstate.cc terminal.cc parseraction.cc terminalfunctions.cc swrite.cc terminalframebuffer.cc terminaldispatcher.cc terminaluserinput.cc terminaldisplay.cc network.cc ocb.cc base64.cc crypto.cc networktransport.cc transportfragment.cc user.cc completeterminal.cc transportsender.cc stmclient.cc terminaloverlay.cc

mosh_client_SOURCES = mosh-client.cc $(source)

mosh_server_SOURCES = mosh-server.cc $(source)
