AM_CPPFLAGS = -I$(srcdir)/../util $(CRYPTO_CFLAGS)
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS) $(CODE_COVERAGE_CXXFLAGS)

noinst_LIBRARIES = libmoshcrypto.a

OCB_SRCS = ae.h
if CRYPTO_LIBRARY_OPENSSL
OCB_SRCS += ocb_openssl.cc
else
OCB_SRCS += ocb_internal.cc
endif

libmoshcrypto_a_SOURCES = \
	$(OCB_SRCS) \
	base64.cc \
	base64.h \
	byteorder.h \
	crypto.cc \
	crypto.h \
	prng.h
