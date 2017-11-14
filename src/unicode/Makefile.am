EXTRA_DIST = \
	make_widths.py edit_widths.py

CLEANFILES = UCD.zip UnicodeData.txt PropList.txt EastAsianWidth.txt

noinst_SCRIPTS=make_widths.py

# This is a set of developer-only targets for rebuilding Unicode
# information in Mosh.
# They're not intended to be run as part of normal automake processing.
# Dependencies which must be provided manually: git python3 unzip

UCD.zip:
	curl -O https://www.unicode.org/Public/10.0.0/ucd/UCD.zip

google-libapps/libdot/third_party/wcwidth/ranges.py:
	git submodule init
	git submodule update

ucd/PropList.txt ucd/UnicodeData.txt ucd/EastAsianWidth.txt: UCD.zip
	mkdir -p ucd
	unzip -D -o -d ucd UCD.zip
	ln -fs ucd/PropList.txt ucd/UnicodeData.txt ucd/EastAsianWidth.txt .

eaw_narrow_map eaw_wide_map: ucd/PropList.txt ucd/UnicodeData.txt ucd/EastAsianWidth.txt make_widths.py google-libapps/libdot/third_party/wcwidth/ranges.py
	PYTHONPATH=$(PWD)/google-libapps/libdot/third_party/wcwidth/ python3 -Bs ./make_widths.py --narrow-table=eaw_narrow_map --wide-table=eaw_wide_map

edit-tables: eaw_narrow_map edit_widths.py
	python3 ./edit_widths.py ./eaw_narrow_map ../util/chwidth_tables.cc chwidth_default_table

clean-local: clean-local-check
.PHONY: clean-local-check
clean-local-check:
	rm -rf ucd/
