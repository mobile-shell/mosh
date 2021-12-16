AM_CPPFLAGS = -I$(srcdir)/../util $(CRYPTO_CFLAGS)
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
