EXTRA_DIST = wrap-compiler-for-flag-check mosh.pl
if BUILD_CLIENT
  bin_SCRIPTS = mosh
endif
CLEANFILES = $(bin_SCRIPTS)

mosh:	mosh.pl ../VERSION Makefile
	@sed -e "s/\@VERSION\@/`cat ../VERSION`/" -e "s/\@PACKAGE_STRING\@/@PACKAGE_STRING@/" $(srcdir)/mosh.pl > mosh
	@chmod a+x mosh
