noinst_LIBRARIES = libmosh_windows.a

if WINDOWS
libmosh_windows_a_SOURCES = \
	pty_windows.h \
	pty_windows.cc \
	select_windows.h \
	select_windows.cc \
	signal_windows.h \
	signal_windows.cc

libmosh_windows_a_CXXFLAGS = -std=c++17 $(PICKY_CXXFLAGS) $(WARNING_CXXFLAGS)
endif
