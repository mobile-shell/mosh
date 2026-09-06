#!/usr/bin/env python3
"""Exercise packaging with real Mach-O dependency graphs on macOS."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("bundle_runtime", Path(__file__).with_name("bundle-runtime.py"))
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)


@unittest.skipUnless(sys.platform == "darwin", "requires Mach-O tools")
class BundleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mosh bundle test ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.prefix = self.root / "brew"
        self.formula = self.prefix / "Cellar/example/1.0"
        self.lib = self.formula / "lib"
        self.lib.mkdir(parents=True)
        (self.formula / "INSTALL_RECEIPT.json").write_text("{}")
        (self.formula / "LICENSE").write_text("fixture license\n")
        self.source = self.root / "source"
        (self.source / "src/frontend").mkdir(parents=True)
        (self.source / "scripts").mkdir()
        (self.source / "COPYING").write_text("fixture copying\n")
        (self.source / "scripts/mosh").write_text(
            "#!/usr/bin/env perl\nmy $client = 'mosh-client';\nexec $client, @ARGV;\n")
        self.output = self.root / "output"

    def compile(self, code, output, *flags):
        subprocess.run(["clang", "-x", "c", "-", "-o", str(output),
                        "-Wl,-headerpad_max_install_names", *flags],
                       input=code, text=True, check=True, capture_output=True)

    def build_graph(self):
        self.compile("int answer(void) { return 42; }", self.lib / "libanswer.dylib",
                     "-dynamiclib", "-Wl,-install_name,@rpath/libanswer.dylib")
        self.compile("extern int answer(void); int indirect(void) { return answer(); }",
                     self.lib / "libindirect.dylib", "-dynamiclib",
                     "-Wl,-install_name," + str(self.lib / "libindirect.dylib"),
                     "-Wl,-rpath,@loader_path", "-L" + str(self.lib), "-lanswer")
        code = '#include <stdio.h>\nextern int indirect(void); int main(void) { printf("%d\\n", indirect()); return 0; }'
        self.compile(code, self.source / "src/frontend/mosh-client",
                     "-L" + str(self.lib), "-lindirect", "-Wl,-rpath," + str(self.lib))
        shutil.copy2(self.source / "src/frontend/mosh-client", self.source / "src/frontend/mosh-server")

    def test_relocation_without_original_libraries_and_path_shadowing(self):
        self.build_graph()
        bundle.package(self.source, self.output, self.prefix)
        relocated = self.root / "relocated bundle"
        self.output.rename(relocated)
        self.prefix.rename(self.root / "unavailable-brew")
        decoy = self.root / "decoy"
        decoy.mkdir()
        (decoy / "mosh-client").write_text("#!/bin/sh\nexit 99\n")
        (decoy / "mosh-client").chmod(0o755)
        entry = self.root / "mosh"
        entry.symlink_to(relocated / "bin/mosh")
        result = subprocess.check_output([str(entry), "--version"], text=True,
                                         env={**os.environ, "PATH": str(decoy) + ":/usr/bin:/bin"})
        self.assertEqual(result.strip(), "42")
        self.assertEqual((relocated / "licenses/example/LICENSE").read_text(), "fixture license\n")
        for binary in list((relocated / "lib").iterdir()) + [relocated / "bin/mosh-client"]:
            self.assertTrue(all(not d.startswith(str(self.prefix)) for d in bundle.dependencies(binary)))
            self.assertTrue(all(not r.startswith(str(self.prefix)) for r in bundle.rpaths(binary)))
            bundle.run("codesign", "--verify", "--strict", str(binary))

    def test_existing_destination_is_preserved(self):
        self.output.mkdir()
        sentinel = self.output / "keep"
        sentinel.write_text("existing installation")
        with self.assertRaises(FileExistsError):
            bundle.package(self.source, self.output, self.prefix)
        self.assertEqual(sentinel.read_text(), "existing installation")

    def test_rejects_dependency_outside_selected_prefix(self):
        external = self.root / "external.dylib"
        external.write_bytes(b"not a bundled library")
        with self.assertRaisesRegex(RuntimeError, "Non-Homebrew"):
            bundle.resolve_dependency(str(external), self.root / "binary", self.prefix)

    def test_rejects_ambiguous_rpath(self):
        self.build_graph()
        other = self.prefix / "other"
        other.mkdir()
        shutil.copy2(self.lib / "libanswer.dylib", other / "libanswer.dylib")
        library = self.lib / "libindirect.dylib"
        bundle.run("install_name_tool", "-add_rpath", str(other), str(library))
        with self.assertRaisesRegex(RuntimeError, "unambiguously"):
            bundle.resolve_dependency("@rpath/libanswer.dylib", library, self.prefix)


if __name__ == "__main__":
    unittest.main()
