AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) -fno-default-inline -pipe
AM_LDFLAGS  = $(HARDEN_LDFLAGS)

if BUILD_TESTS
  noinst_PROGRAMS = ocb-aes encrypt-decrypt
endif

ocb_aes_SOURCES = ocb-aes.cc test_utils.cc
ocb_aes_CPPFLAGS = -I$(srcdir)/../crypto -I$(srcdir)/../util
ocb_aes_LDADD = ../crypto/libmoshcrypto.a ../util/libmoshutil.a

encrypt_decrypt_SOURCES = encrypt-decrypt.cc test_utils.cc
encrypt_decrypt_CPPFLAGS = -I$(srcdir)/../crypto -I$(srcdir)/../util
encrypt_decrypt_LDADD = ../crypto/libmoshcrypto.a ../util/libmoshutil.a
