#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#   Mosh: the mobile shell
#   Copyright 2017 Keith Winstein
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#   In addition, as a special exception, the copyright holders give
#   permission to link the code of portions of this program with the
#   OpenSSL library under certain conditions as described in each
#   individual source file, and distribute linked combinations including
#   the two.
#
#   You must obey the GNU General Public License in all respects for all
#   of the code used other than OpenSSL. If you modify file(s) with this
#   exception, you may extend this exception to your version of the
#   file(s), but you are not obligated to do so. If you do not wish to do
#   so, delete this exception statement from your version. If you delete
#   this exception statement from all source files in the program, then
#   also delete it here.

"""Helper for generating width tables for Mosh, from Unicode data
files.

Since the Google hterm/nassh projects have already done the hard work
of figuring out what a Unicode wcwidth table should be, we stand on
their shoulders to generate our Unicode widths table.

The libapps distribution includes a JS reimplementation of the Markus
Kuhn wcwidth implementation, which uses binary-search tables, and can
regenerate them from Unicode source data files.

This script takes the binary-search tables, transforms them into a
useful form for Mosh, and in turn edits src/utils/chwidth_tables.cc to
bring it up to date.

"""

# Library modules
import argparse
import re
import sys

# Unicode data from Google libapps
import ranges

# The entire Unicode space is defined as [0..10ffff].
unicode_codespace = 0x110000


valid_planes = [
    [0x0000, 0x14fff],
    [0x16000, 0x18fff],
    [0x1b000, 0x1bfff],
    [0x1d000, 0x2ffff],
    [0xe0000, 0x10ffff],
];

# Invalid code points for UTF-8 wcwidth.
# Mosh doesn't support combining surrogate code points encoded in
# UTF-8.  It'll inevitably happen somewhere.
invalid_code_points = [
    [0, 0x1f], # controls
    [0x7f, 0x9f], # DEL + 8 bit controls
    [0xd800, 0xdfff], # surrogate code points
];

# Write a list of ranges onto a flat bytemap.
def apply_table_to_map(map, table, width):
    byte = ord(width)
    for pair in table:
        (first, last) = pair
        for i in range(first, last + 1):
            map[i] = byte

def compose_base_set(uni_db):
    all = set()
    for i in uni_db:
        all |= uni_db[i]
    return all

def main(argv):
    argparser  = argparse.ArgumentParser(description='Generate Mosh character maps from Unicode data files')
    argparser.add_argument('--narrow-table',
                           help='Name for normal (ambiguous-narrow) map output file',
                           default='eaw_narrow_map')
    argparser.add_argument('--wide-table',
                           help='Name for wide (ambiguous-wide) map output file',
                           default='eaw_wide_map')
    opts = argparser.parse_args(argv)

    prop_db = ranges.load_proplist()
    uni_db = ranges.load_unicode_data()
    cjk_db = ranges.load_east_asian()

    codepoints = ranges.gen_table(compose_base_set(uni_db))
    combining = ranges.gen_combining(uni_db, prop_db)
    east_asian = ranges.gen_east_asian(cjk_db)
    east_asian_ambiguous = ranges.gen_east_asian_ambiguous(cjk_db)

    # This set of overlays should result in a wcwidth map identical to
    # libdot's, except that undefined codepoints return -1, not 1.
    basemap = bytearray(b'-' * unicode_codespace)
    apply_table_to_map(basemap, codepoints, '1')
    apply_table_to_map(basemap, east_asian, '2')
    apply_table_to_map(basemap, combining, '0')

    # The following are Mosh overlays that diverge from libdot.

    # In our wcwidth(), control and surrogate codepoints are invalid
    # characters.
    apply_table_to_map(basemap, invalid_code_points, '-')
    # hterm's tables have SOFT HYPHEN as width 0, but most terminal emulators
    # use width 1.
    # basemap[0x00AD] = ord('1')

    # Apple sets the PUA to width 1, and SPUA-A/SPUA-B to width 2.
    # This seems reasonable and more useful then setting them to 0.
    apply_table_to_map(basemap, [[0xe800, 0xf7ff]], '1')
    apply_table_to_map(basemap,
                       [[0x0f0000, 0x0ffffd], [0x100000, 0x10fffd]],
                       '2')

    # And this is the east asian width overlay from libdot.
    eawwidemap = basemap.copy()
    apply_table_to_map(eawwidemap, east_asian_ambiguous, '2')

    with open(opts.narrow_table, 'w') as f:
        f.write(basemap.decode())

    with open(opts.wide_table, 'w') as f:
        f.write(eawwidemap.decode())

    return None

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
