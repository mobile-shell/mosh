AM_CPPFLAGS = -I$(top_builddir)/protobufs/ -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -D_BSD_SOURCE
AM_CXXFLAGS = -g -O2 --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-default-inline -pipe
LIBS = `pkg-config --libs protobuf-lite`
LDADD = -lutil -lcrypto -lrt -lm $(top_builddir)/protobufs/libmoshprotos.a

bin_PROGRAMS = mosh-client mosh-server

source = parser.cpp parserstate.cpp terminal.cpp parseraction.cpp terminalfunctions.cpp swrite.cpp terminalframebuffer.cpp terminaldispatcher.cpp terminaluserinput.cpp terminaldisplay.cpp network.cpp ocb.cpp base64.cpp crypto.cpp networktransport.cpp transportfragment.cpp user.cpp completeterminal.cpp transportsender.cpp stmclient.cpp terminaloverlay.cpp

mosh_client_SOURCES = mosh-client.cpp $(source)

mosh_server_SOURCES = mosh-server.cpp $(source)
