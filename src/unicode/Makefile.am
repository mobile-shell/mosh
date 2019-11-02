EXTRA_DIST = \
	make_widths.py edit_widths.py

CLEANFILES = UCD.zip UnicodeData.txt PropList.txt EastAsianWidth.txt eaw_narrow_map eaw_wide_map eaw_delta_map

noinst_SCRIPTS=make_widths.py

# This is a set of developer-only targets for rebuilding Unicode
# information in Mosh.
# They're not intended to be run as part of normal automake processing.
# Dependencies which must be provided manually: git python3 unzip

PYTHONPATH=$(PWD)/google-libapps/libdot/third_party/wcwidth/:$(PWD)

UCD.zip:
	curl -O https://www.unicode.org/Public/11.0.0/ucd/UCD.zip

google-libapps/libdot/third_party/wcwidth/ranges.py:
	git submodule init
	git submodule update

PropList.txt UnicodeData.txt EastAsianWidth.txt: UCD.zip
	unzip -o UCD.zip $@
	touch -c $@

eaw_narrow_map: PropList.txt UnicodeData.txt EastAsianWidth.txt edit_widths.py make_widths.py google-libapps/libdot/third_party/wcwidth/ranges.py
	PYTHONPATH=$(PYTHONPATH) python3 -Bs ./make_widths.py --narrow-table=eaw_narrow_map --wide-table=eaw_wide_map --delta-table=eaw_delta_map
eaw_wide_map: eaw_narrow_map
eaw_delta_map: eaw_narrow_map

edit-tables: eaw_narrow_map eaw_delta_map edit_widths.py
	python3 -Bs ./edit_widths.py --edit ./eaw_narrow_map ../util/chwidth_tables.cc chwidth_default_table
	python3 -Bs ./edit_widths.py --edit ./eaw_delta_map ../util/chwidth_tables.cc chwidth_eaw_delta_table
