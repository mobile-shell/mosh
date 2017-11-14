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
import codecs
import re
import sys

# The entire Unicode space is defined as [0..10ffff].
unicode_codespace = 0x110000

def main(argv):
    if len(argv) != 3:
        sys.stderr.write("error:  need 3 args\n")
        raise Exception

    (mapfile, sourcefile, table_name) = argv

    basemap = None
    with codecs.open(mapfile, encoding='ascii') as f:
        basemap = bytearray(f.read(), 'ascii')

    # XXX allow smaller?
    if len(basemap) != unicode_codespace:
        sys.stderr.write("error: bad length for map\n")
        raise Exception

    # Generate a new table as a string.
    lines = "%s[] =\n" % table_name + "{\n"
    for i in range(0, unicode_codespace, 32):
        lines += '    ' + ",".join([str(i) for i in basemap[i:i+32]]) + ','
        if i % 256 == 0:
            lines += " // {:06x}".format(i)
        lines += "\n"
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

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
