AM_CPPFLAGS = -I$(srcdir)/../util $(OPENSSL_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS)

noinst_LIBRARIES = libmoshcrypto.a

OCB_SRCS = \
	ae.h \
	ocb.cc

libmoshcrypto_a_SOURCES = \
	$(OCB_SRCS) \
	base64.cc \
	base64.h \
	byteorder.h \
	crypto.cc \
	crypto.h \
	prng.h

check_PROGRAMS = ocb-test
TESTS = ocb-test

ocb_test_SOURCES = $(OCB_SRCS)
ocb_test_CPPFLAGS = -DOCB_TEST_PROGRAM
ocb_test_LDADD = $(OPENSSL_LIBS)
