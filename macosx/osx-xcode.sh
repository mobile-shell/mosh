#!/bin/sh

#
# OS X and Xcode support script.
#

#
# Describe the OS X and Xcode installation, patches, etc as best as possible.
#
# Beware: System Profiler dumps significant private and security information.
#
describe()
{
    # Most of the XML in this report is plist files, which can be read more easily with plutil -p
    pkgutil --pkgs-plist > packages-plist.xml
    mkdir package-info-plist/
    for i in $(pkgutil --pkgs); do pkgutil --pkg-info-plist=$i > package-info-plist/$i.xml; done
    xcodebuild -version > xcodebuild-version.txt
    # CLT info can be found in package-info-plist/com.apple.pkg.CLTools_Executables.xml
    xcode-select --print-path > xcode-path.txt
    # System Profiler's XML can be read more easily with plutil -p, or
    # opened with the System Profiler GUI.
    system_profiler -xml -detailLevel full > system-profile.spx 2>/dev/null
}

#
# Do something.
#
set -e
"$@"
