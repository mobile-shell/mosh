AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS)

noinst_LIBRARIES = libmoshutil.a

libmoshutil_a_SOURCES = locale_utils.cc locale_utils.h swrite.cc swrite.h dos_assert.h fatal_assert.h sigfd.h select.h

if !USE_LIBSTDDJB
  libmoshutil_a_SOURCES += sigfd.cc
endif
