SUBDIRS = protobufs .

AM_CPPFLAGS = -Iprotobufs -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -D_BSD_SOURCE
AM_CXXFLAGS = -g -O2 --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-default-inline -pipe
LIBS = `pkg-config --libs protobuf-lite`
LDADD = -lutil -lrt -lm protobufs/libmoshprotos.a

bin_PROGRAMS = mosh-client mosh-server

source = ae.h base64.cc base64.h completeterminal.cc completeterminal.h crypto.cc crypto.h dos_assert.h network.cc network.h networktransport.cc networktransport.h ocb.cc parseraction.cc parseraction.h parser.cc parser.h parserstate.cc parserstatefamily.h parserstate.h parsertransition.h stmclient.cc stmclient.h swrite.cc swrite.h terminal.cc terminaldispatcher.cc terminaldispatcher.h terminaldisplay.cc terminaldisplay.h terminalframebuffer.cc terminalframebuffer.h terminalfunctions.cc terminal.h terminaloverlay.cc terminaloverlay.h terminaluserinput.cc terminaluserinput.h transportfragment.cc transportfragment.h transportsender.cc transportsender.h transportstate.h user.cc user.h rijndael-alg-fst.cc rijndael-alg-fst.h

mosh_client_SOURCES = mosh-client.cc $(source)

mosh_server_SOURCES = mosh-server.cc $(source)
