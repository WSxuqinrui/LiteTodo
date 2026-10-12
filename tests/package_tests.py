"""用临时发布目录验证重复打包不会收录用户数据。"""
import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/package.py"
spec = importlib.util.spec_from_file_location("package_probe", SCRIPT)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

class PackageSafetyTest(unittest.TestCase):
    def test_rebuild_excludes_data_and_preserves_previous_directory(self):
        with tempfile.TemporaryDirectory(prefix="litetodo-package-test-") as tmp:
            root = Path(tmp)
            for name in ("README.md", "CHANGELOG.md", "THIRD_PARTY_NOTICES.md", "docs/interaction-reference.md", "docs/revision-design.md",
                         "vendor/fltk-release-1.4.5/COPYING", "docs/licenses/example.txt", "LiteTodo.exe"):
                file = root / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text("test payload", encoding="utf-8")
            previous = root / "dist/windows-x64/data/tasks.sqlite3"
            previous.parent.mkdir(parents=True)
            previous.write_bytes(b"private data must stay outside release")
            (root / "CMakeLists.txt").write_text("project(LiteTodo VERSION 1.2.3 LANGUAGES C CXX)\n", encoding="utf-8")
            with patch.object(package, "ROOT", root), patch("sys.argv", ["package.py", "--binary", str(root / "LiteTodo.exe"), "--platform", "windows-x64"]):
                package.main()
            archive = root / "dist/LiteTodo-1.2.3-windows-x64.zip"
            with zipfile.ZipFile(archive) as zipped:
                self.assertIn("CHANGELOG.md", zipped.namelist())
                self.assertFalse(any(name.startswith("data/") for name in zipped.namelist()))
                report = json.loads(zipped.read("SIZE.json"))
                self.assertFalse(any(row["file"].startswith("data/") for row in report["files"]))
            self.assertTrue(any(path.read_bytes() == b"private data must stay outside release" for path in (root / "dist").glob("windows-x64-previous-*/data/tasks.sqlite3")))

if __name__ == "__main__":
    unittest.main()
