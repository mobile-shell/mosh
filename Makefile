source = parse.cpp parserstate.cpp parser.cpp templates.cpp
objects = parserstate.o parser.o templates.o
executables = parse

CPP = g++
CPPFLAGS = -g --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-implicit-templates -pipe -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500 -D_GNU_SOURCE
LIBS =

all: $(executables)

parse: parse.o $(objects)
	$(CPP) $(CPPFLAGS) -o $@ $+ $(LIBS)

templates.o: templates.cpp
	$(CPP) $(CPPFLAGS) -frepo -c -o $@ $<

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

include depend

depend: $(source)
	$(CPP) $(INCLUDES) -MM $(source) > depend

.PHONY: clean
clean:
	-rm -f $(executables) depend *.o *.rpo
