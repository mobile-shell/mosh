#!/usr/bin/env python3
"""Bundle a native macOS Mosh build with private Homebrew runtime libraries."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


SYSTEM_PATHS = ("/usr/lib/", "/System/Library/")


def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.PIPE).strip()


def dependencies(path):
    return [line.strip().split(" (compatibility version", 1)[0]
            for line in run("otool", "-L", str(path)).splitlines()[1:]]


def rpaths(path):
    return re.findall(r"cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (.+?) \(offset",
                      run("otool", "-l", str(path)))


def resolve_dependency(dep, original, brew_prefix):
    if dep.startswith("@rpath/"):
        candidates = []
        for entry in rpaths(original):
            entry = entry.replace("@loader_path", str(original.parent))
            if not entry.startswith("/"):
                continue
            candidate = Path(entry) / dep[len("@rpath/"):]
            if candidate.is_file():
                candidates.append(candidate.resolve())
        if len(set(candidates)) != 1:
            raise RuntimeError(f"Cannot unambiguously resolve {dep} in {original}")
        resolved = candidates[0]
    elif dep.startswith("@loader_path/"):
        resolved = (original.parent / dep[len("@loader_path/"):]).resolve(strict=True)
    elif dep.startswith("/"):
        resolved = Path(dep).resolve(strict=True)
    else:
        raise RuntimeError(f"Unsupported dependency {dep} in {original}")
    if brew_prefix not in resolved.parents:
        raise RuntimeError(f"Non-Homebrew dependency requires review: {resolved}")
    return resolved


def package(source, destination, brew_prefix):
    """Create a new bundle; never overwrite an existing installation."""
    destination.mkdir(parents=True, exist_ok=False)
    for directory in ("bin", "lib", "licenses"):
        (destination / directory).mkdir()
    queue = []
    libraries = {}
    formulas = set()
    for name in ("mosh-client", "mosh-server"):
        original = source / "src/frontend" / name
        target = destination / "bin" / name
        shutil.copy2(original, target)
        queue.append((original, target))
    launcher = (source / "scripts/mosh").read_text()
    old = "my $client = 'mosh-client';"
    if launcher.count(old) != 1:
        raise RuntimeError("Unexpected launcher: cannot select the bundled client")
    launcher = launcher.replace("#!/usr/bin/env perl", "#!/usr/bin/perl", 1)
    launcher = launcher.replace(old, '''use FindBin qw($RealBin);
my $client = "$RealBin/mosh-client";
if (-d "$RealBin/../share/terminfo") {
  $ENV{'TERMINFO_DIRS'} = "$RealBin/../share/terminfo:" . ($ENV{'TERMINFO_DIRS'} // '');
}''')
    (destination / "bin/mosh").write_text(launcher)
    (destination / "bin/mosh").chmod(0o755)
    processed = []
    while queue:
        original, target = queue.pop(0)
        changes = []
        identity = None
        if target.parent.name == "lib":
            identities = run("otool", "-D", str(original)).splitlines()[1:]
            identity = identities[0] if identities else None
        for dep in dependencies(original):
            if dep == identity or dep.startswith(SYSTEM_PATHS):
                continue
            library = resolve_dependency(dep, original, brew_prefix)
            previous = libraries.get(library.name)
            if previous is not None and previous != library:
                raise RuntimeError(f"Library filename collision: {previous} and {library}")
            bundled = destination / "lib" / library.name
            if previous is None:
                libraries[library.name] = library
                shutil.copy2(library, bundled)
                bundled.chmod(0o755)
                queue.append((library, bundled))
                formula = next((p for p in library.parents
                                if (p / "INSTALL_RECEIPT.json").is_file()), None)
                if formula is not None:
                    formulas.add(formula)
            relative = os.path.relpath(bundled, target.parent)
            changes.extend(["-change", dep, "@loader_path/" + relative])
        if target.parent.name == "lib":
            changes.extend(["-id", "@loader_path/" + target.name])
        for entry in set(rpaths(original)):
            if entry.startswith(str(brew_prefix) + "/"):
                changes.extend(["-delete_rpath", entry])
        if changes:
            run("install_name_tool", *changes, str(target))
        processed.append(target)
    for formula in sorted(formulas):
        licenses = destination / "licenses" / formula.parent.name
        licenses.mkdir(exist_ok=True)
        for path in formula.iterdir():
            if path.is_file() and path.name.upper().startswith(("LICENSE", "COPYING", "NOTICE", "AUTHORS")):
                shutil.copy2(path, licenses / path.name)
        terminfo = formula / "share/terminfo"
        if terminfo.is_dir():
            shutil.copytree(terminfo, destination / "share/terminfo", symlinks=True)
    shutil.copy2(source / "COPYING", destination / "COPYING")
    for target in processed:
        run("codesign", "--force", "--sign", "-", str(target))
        run("codesign", "--verify", "--strict", str(target))
        for dep in dependencies(target):
            if dep.startswith("@loader_path/"):
                resolved = (target.parent / dep[len("@loader_path/"):]).resolve(strict=True)
                if destination not in resolved.parents:
                    raise RuntimeError(f"Dependency escapes bundle: {dep}")
            elif not dep.startswith(SYSTEM_PATHS):
                raise RuntimeError(f"External runtime dependency remains: {dep}")
    manifest = {"libraries": {name: str(path.relative_to(brew_prefix))
                              for name, path in sorted(libraries.items())}}
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    for name in ("mosh-client", "mosh-server"):
        print(run(str(destination / "bin" / name), "--version"))
    print(f"Verified {len(processed)} Mach-O files; no external Homebrew runtime dependencies.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path, help="Configured and compiled native Mosh build directory")
    parser.add_argument("destination", type=Path, help="New directory to create (must not exist)")
    parser.add_argument("--brew-prefix", type=Path, help="Homebrew prefix (default: brew --prefix)")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("This tool requires macOS and Xcode command line tools")
    prefix = args.brew_prefix or Path(run("brew", "--prefix"))
    try:
        package(args.build.resolve(), args.destination.resolve(), prefix.resolve())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Packaging failed: {error}\n")


if __name__ == "__main__":
    main()
