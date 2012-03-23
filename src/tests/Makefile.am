AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) -fno-default-inline -pipe

if BUILD_TESTS
  noinst_PROGRAMS = ocb-aes
endif

ocb_aes_SOURCES = ocb-aes.cc test_utils.cc
ocb_aes_CPPFLAGS = -I$(srcdir)/../crypto -I$(srcdir)/../util
ocb_aes_LDADD = ../crypto/libmoshcrypto.a ../util/libmoshutil.a
