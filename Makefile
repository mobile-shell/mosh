source = parse.cpp parserstate.cpp parser.cpp templates.cpp terminal.cpp termemu.cpp parseraction.cpp terminalfunctions.cpp swrite.cpp terminalframebuffer.cpp terminaldispatcher.cpp terminaluserinput.cpp terminaldisplay.cpp network.cpp ntester.cpp
objects = parserstate.o parser.o templates.o terminal.o parseraction.o terminalfunctions.o swrite.o terminalframebuffer.o terminaldispatcher.o terminaluserinput.o terminaldisplay.o network.o
repos = templates.rpo
executables = parse termemu ntester

CXX = g++
CXXFLAGS = -g --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-implicit-templates -fno-default-inline -pipe -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500 -D_GNU_SOURCE
LIBS = -lutil

all: $(executables)

parse: parse.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ $+ $(LIBS)

termemu: termemu.o $(objects) parse # serialize link steps because of -frepo
	$(CXX) $(CXXFLAGS) -o $@ termemu.o $(objects) $(LIBS)

ntester: ntester.o $(objects) termemu # serialize link steps because of -frepo
	$(CXX) $(CXXFLAGS) -o $@ ntester.o $(objects) $(LIBS)

templates.o: templates.cpp
	$(CXX) $(CXXFLAGS) -frepo -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

-include depend

depend: $(source)
	$(CXX) $(INCLUDES) -MM $(source) > depend

.PHONY: clean
clean:
	-rm -f $(executables) depend *.o *.rpo
