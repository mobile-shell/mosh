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

"""Helper for editing width tables into Mosh source

Takes three arguments: input map file, output C++ source file, and
array name.  Replaces the contents of the named array in the C++
source with the input map file.
"""

# Library modules
import argparse
import re
import sys

# The entire Unicode space is defined as [0..10ffff].
unicode_codespace = 0x110000

def make_map(map):
    # Generate a list of runs.
    runs = []
    range_len = 0
    range_type = None
    for i in range(0, len(map)):
        if map[i] != range_type:
            if range_type != None:
                runs.append('{} {}\n'.format(range_len, chr(range_type)))
            range_type = map[i]
            range_len = 0
        range_len += 1
    if range_len != 0:
        runs.append('{} {}\n'.format(range_len, chr(range_type)))
    return runs

def write_maps(map, name):
    # Write list of runs.
    with open(name, 'w') as runs_text:
        runs_text.writelines(make_map(map))
    return None

def read_runs(input):
    # Read a list of runs from a readline()-able input into an array of pairs.
    runs = []
    eat_ws_comments = re.compile(' *#.*$')
    for line in input.readlines():
        line = re.sub(eat_ws_comments, '', line.rstrip())
        if line == '':
            continue
        words = line.split()
        # ignore excess words, throw on insufficient words
        range_len = int(words[0])
        range_type = words[1]
        runs.append((range_len, range_type))
    return runs

def read_map(name):
    # Read a list of runs into a flat map.  Returns the flatmap as a bytearray.
    with open(name, 'r') as runs_text:
        runs = read_runs(runs_text)
    flatmap = bytearray()
    for (range_len, range_type) in runs:
        flatmap += bytearray(range_type * range_len, 'latin_1')
    return map

def make_merge(basename, deltaname, outname):
    base = None
    delta = None
    base = read_map(basename)
    delta = read_map(deltaname)
    out = base.copy()
    for i in range(0, min(len(delta), len(base))):
        if delta[i] != ord('='):
            out[i] = delta[i]
    write_maps(out, outname)

def make_delta(basename, deltaname, outname):
    base = None
    delta = None
    base = read_map(basename)
    delta = read_map(deltaname)
    out = bytearray(unicode_codespace)
    for i in range(0, max(len(delta), len(base))):
        if base[i] == delta[i]:
            out[i] = ord('=')
        else:
            out[i] = delta[i]
    write_maps(out, outname)

def edit_tables(argv):
    if len(argv) != 3:
        sys.stderr.write("error:  need 3 args\n")
        raise Exception

    (mapfile, sourcefile, table_name) = argv

    basemap = None
    with open(mapfile, 'r') as f:
        basemap = read_runs(f)

    # Generate a new table as a string.
    lines = "%s[] =\n" % table_name + "{\n"
    for (range_len, range_type) in basemap:
        lines += '    "{} {}\\\\n"\n'.format(range_len, range_type)
    lines += "};\n"
    text=''
    # Edit chwidths.cc with the new table.
    with open(sourcefile, 'r') as f:
        text = f.read()
    (text, count) = re.subn(r'%s\[\]\s*=\s*\{.*?\};\s*$' % table_name,
                            lines,
                            text,
                            flags = re.MULTILINE|re.DOTALL)
    if count == 0:
        print ("substitute failed")
        raise Exception
    with open(sourcefile, 'w') as f:
        f.write(text)

    return None

def main(argv):
    argparser  = argparse.ArgumentParser(description='Generate Mosh character maps from Unicode data files')
    argparser.add_argument('--merge',
                           help='Merge two tables into one by overlaying the second on the first',
                           action='store_true')
    argparser.add_argument('--delta',
                           help='Diff two tables by generating a delta',
                           action='store_true')
    argparser.add_argument('--edit',
                           help='Edit table into a C source file',
                           action='store_true')
    argparser.add_argument('files', nargs='*')
    opts = argparser.parse_args(argv)

    if opts.delta:
        make_delta(opts.files[0], opts.files[1], opts.files[2])
    elif opts.merge:
        make_merge(opts.files[0], opts.files[1], opts.files[2])
    elif opts.edit:
        edit_tables(opts.files)
    return None

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
