proto = userinput.proto hostinput.proto transportinstruction.proto
source = parse.cpp parserstate.cpp parser.cpp terminal.cpp termemu.cpp parseraction.cpp terminalfunctions.cpp swrite.cpp terminalframebuffer.cpp terminaldispatcher.cpp terminaluserinput.cpp terminaldisplay.cpp network.cpp ntester.cpp ocb.cpp base64.cpp encrypt.cpp decrypt.cpp crypto.cpp networktransport.cpp transportfragment.cpp user.cpp userinput.pb.cc completeterminal.cpp mosh-server.cpp mosh.cpp transportinstruction.pb.cc transportsender.cpp stmclient.cpp terminaloverlay.cpp hostinput.pb.cc
objects = parserstate.o parser.o terminal.o parseraction.o terminalfunctions.o swrite.o terminalframebuffer.o terminaldispatcher.o terminaluserinput.o terminaldisplay.o network.o ocb.o base64.o crypto.o networktransport.o transportfragment.o user.o userinput.pb.o completeterminal.o transportinstruction.pb.o transportsender.o stmclient.o terminaloverlay.o hostinput.pb.o
executables = parse termemu ntester encrypt decrypt mosh-server mosh

CXX = g++
CXXFLAGS = -g -O2 --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-default-inline -pipe -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -D_BSD_SOURCE
LIBS = -lutil -lcrypto -lrt -lm -lprotobuf-lite
PROTOC = protoc

all: $(executables)

parse: parse.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ $+ $(LIBS)

termemu: termemu.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ termemu.o $(objects) $(LIBS)

ntester: ntester.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ ntester.o $(objects) $(LIBS)

encrypt: encrypt.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ encrypt.o $(objects) $(LIBS)

decrypt: decrypt.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ decrypt.o $(objects) $(LIBS)

mosh-server: mosh-server.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ mosh-server.o $(objects) $(LIBS)

mosh: mosh.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ mosh.o $(objects) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.pb.cc: %.proto
	$(PROTOC) --cpp_out=. $<

%.pb.o: %.pb.cc
	$(CXX) $(CXXFLAGS) -Wno-effc++ -c -o $@ $<

-include depend

depend: $(source)
	$(CXX) $(INCLUDES) -MM $(source) > depend

.PHONY: clean
clean:
	-rm -f $(executables) depend *.o *.pb.cc *.pb.h
