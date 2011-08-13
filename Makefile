proto = userinput.proto
source = parse.cpp parserstate.cpp parser.cpp templates.cpp terminal.cpp termemu.cpp parseraction.cpp terminalfunctions.cpp swrite.cpp terminalframebuffer.cpp terminaldispatcher.cpp terminaluserinput.cpp terminaldisplay.cpp network.cpp ntester.cpp ocb.cpp base64.cpp encrypt.cpp decrypt.cpp crypto.cpp networktransport.cpp networkinstruction.cpp user.cpp userinput.pb.cc
objects = parserstate.o parser.o templates.o terminal.o parseraction.o terminalfunctions.o swrite.o terminalframebuffer.o terminaldispatcher.o terminaluserinput.o terminaldisplay.o network.o ocb.o base64.o crypto.o networktransport.o networkinstruction.o user.o userinput.pb.o
repos = templates.rpo
executables = parse termemu ntester encrypt decrypt

CXX = g++
CXXFLAGS = -g --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-implicit-templates -fno-default-inline -pipe -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -D_BSD_SOURCE
LIBS = -lutil -lssl -lrt -lm -lprotobuf-lite
PROTOC = protoc

all: $(executables)

parse: parse.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ $+ $(LIBS)

termemu: termemu.o $(objects) parse # serialize link steps because of -frepo
	$(CXX) $(CXXFLAGS) -o $@ termemu.o $(objects) $(LIBS)

ntester: ntester.o $(objects) termemu # serialize link steps because of -frepo
	$(CXX) $(CXXFLAGS) -o $@ ntester.o $(objects) $(LIBS)

encrypt: encrypt.o $(objects) ntester # serialize link steps because of -frepo
	$(CXX) $(CXXFLAGS) -o $@ encrypt.o $(objects) $(LIBS)

decrypt: decrypt.o $(objects) encrypt # serialize link steps because of -frepo
	$(CXX) $(CXXFLAGS) -o $@ decrypt.o $(objects) $(LIBS)

templates.o: templates.cpp
	$(CXX) $(CXXFLAGS) -frepo -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.pb.cc: %.proto
	$(PROTOC) --cpp_out=. $<

%.pb.o: %.pb.cc
	$(CXX) $(CXXFLAGS) -frepo -Wno-effc++ -c -o $@ $<

-include depend

depend: $(source)
	$(CXX) $(INCLUDES) -MM $(source) > depend

.PHONY: clean
clean:
	-rm -f $(executables) depend *.o *.rpo *.pb.cc *.pb.h
