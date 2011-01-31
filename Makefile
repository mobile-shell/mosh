source = parse.cpp parserstate.cpp parser.cpp templates.cpp terminal.cpp termemu.cpp parseraction.cpp terminalcsi.cpp swrite.cpp terminalframebuffer.cpp terminaldispatcher.cpp
objects = parserstate.o parser.o templates.o terminal.o parseraction.o terminalcsi.o swrite.o terminalframebuffer.o terminaldispatcher.o
repos = templates.rpo
executables = parse termemu

CXX = g++
CXXFLAGS = -g --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-implicit-templates -fno-default-inline -pipe -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500 -D_GNU_SOURCE
LIBS = -lutil

all: $(executables)

parse: parse.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ $+ $(LIBS)

termemu: termemu.o $(objects)
	$(CXX) $(CXXFLAGS) -o $@ $+ $(LIBS)

templates.o: templates.cpp
	$(CXX) $(CXXFLAGS) -frepo -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

include depend

depend: $(source)
	$(CXX) $(INCLUDES) -MM $(source) > depend

.PHONY: clean
clean:
	-rm -f $(executables) depend *.o *.rpo
