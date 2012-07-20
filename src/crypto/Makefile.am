AM_CPPFLAGS = -I$(srcdir)/../util $(OPENSSL_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS)

noinst_LIBRARIES = libmoshcrypto.a

libmoshcrypto_a_SOURCES = \
	ae.h \
	base64.cc \
	base64.h \
	byteorder.h \
	crypto.cc \
	crypto.h \
	ocb.cc \
	prng.h
