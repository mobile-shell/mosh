EXTRA_DIST = smoke.test \
	hold-stdin \
	render-test render-test-server-harness \
	render-validate-failure.test \
	render-validate-success.test
AM_CXXFLAGS = $(WARNING_CXXFLAGS) $(PICKY_CXXFLAGS) $(HARDEN_CFLAGS) $(MISC_CXXFLAGS)
AM_LDFLAGS  = $(HARDEN_LDFLAGS)

displaytests = render-validate-success.test render-validate-failure.test
check_PROGRAMS = ocb-aes encrypt-decrypt
# XXX return smoke to the top
TESTS = ocb-aes encrypt-decrypt $(displaytests) smoke.test
XFAIL_TESTS = render-validate-failure.test

ocb_aes_SOURCES = ocb-aes.cc test_utils.cc test_utils.h
ocb_aes_CPPFLAGS = -I$(srcdir)/../crypto -I$(srcdir)/../util
ocb_aes_LDADD = ../crypto/libmoshcrypto.a ../util/libmoshutil.a $(OPENSSL_LIBS)

encrypt_decrypt_SOURCES = encrypt-decrypt.cc test_utils.cc test_utils.h
encrypt_decrypt_CPPFLAGS = -I$(srcdir)/../crypto -I$(srcdir)/../util
encrypt_decrypt_LDADD = ../crypto/libmoshcrypto.a ../util/libmoshutil.a $(OPENSSL_LIBS)

clean-local: clean-local-check
.PHONY: clean-local-check
clean-local-check:
	-for i in $(displaytests); do rm -rf $$i.d/; done
