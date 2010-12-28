source = parse.cpp parserstate.cpp
objects = parserstate.o
executables = parse

CPP = g++
CPPFLAGS = -g --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-implicit-templates -pipe -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500 -D_GNU_SOURCE
LIBS =

all: $(executables)

parse: parse.o $(objects)
	$(CPP) $(CPPFLAGS) -o $@ $+ $(LIBS)

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

include depend

depend: $(source)
	$(CPP) $(INCLUDES) -MM $(source) > depend

.PHONY: clean
clean:
	-rm -f $(executables) depend *.o *.rpo
