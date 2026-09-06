# Private runtime bundle for native macOS builds

A Mosh binary compiled against Homebrew libraries can stop launching after
`brew upgrade` replaces a versioned Protobuf or Abseil dylib. Unlinking the
Homebrew Mosh formula does not solve that dependency failure.

`bundle-runtime.py` packages an already compiled native Mosh build with private
copies of its Homebrew dylibs. It rewrites the dependency graph to use paths
relative to each binary, removes absolute Homebrew runtime search paths, and
ad-hoc signs and verifies the modified Mach-O files. System libraries remain
system dependencies. The generated Perl launcher selects its sibling
`mosh-client`, including when invoked through a symlink, rather than relying
on PATH to select a compatible client.

The helper is an optional native-build path. It does not replace `build.sh`,
produce a universal installer, notarize a release, or change the Mosh protocol.
Python 3.9 or newer and Xcode command line tools are required for packaging;
the resulting runtime uses macOS's Perl and does not require Python.

## Build and package

In a source checkout, install the normal build prerequisites documented in
the main README. Then build with the installed Protobuf compiler and libraries:

```sh
./autogen.sh
./configure CXXFLAGS='-O2 -std=c++17'
make -j4
make check
python3 macosx/bundle-runtime.py . /absolute/path/to/new-mosh-bundle
```

For an existing configured build, clean old object files and regenerate
Protobuf sources before rebuilding against a new Protobuf version. The
destination must not exist; the helper will not overwrite an installation.
A failed packaging operation can leave a partial destination for inspection.
Use a new destination for a retry.

Homebrew's prefix is discovered with `brew --prefix`, or can be supplied with
`--brew-prefix`. Dependencies outside that prefix and Apple's system libraries
are rejected rather than silently omitted. Ambiguous or unsupported runtime
search paths are also rejected for manual review.

Dependency license files are copied from the Homebrew formula directories.
When a dependency supplies a terminfo database, it is included and the launcher
adds it to `TERMINFO_DIRS`. The bundle records the original relative library
paths in `manifest.json` for maintenance.

Run `bin/mosh --version` and test a connection using the bundle's `bin/mosh`
before installing it. Keep the complete bundle together when relocating it.
Point a `mosh` symlink in your PATH at `bin/mosh`; optional `mosh-client` and
`mosh-server` symlinks can point at their corresponding binaries. Check
`command -v mosh` to ensure another installation is not shadowing it.

If you are replacing a Homebrew Mosh installation, uninstall that formula
only after validating the custom installation. Reconnect existing sessions to
use the new client. Private libraries are intentionally insulated from
Homebrew upgrades, so rebuild the bundle to receive dependency security fixes.
This helper does not change any Homebrew package or shell configuration.

## Packaging tests

```sh
python3 macosx/test-bundle-runtime.py
```

The tests compile small native libraries and an executable, package a
transitive `@rpath` dependency graph, move the bundle to a path containing
spaces, hide the original library prefix, and run the launcher through a
symlink with a competing client on PATH. They also check signed binaries,
license preservation, existing-destination protection, and rejection of
external or ambiguous dependencies. These tests do not replace Mosh's own
connection and terminal tests.
