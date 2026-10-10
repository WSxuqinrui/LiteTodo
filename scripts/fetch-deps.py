"""下载固定依赖并校验 SHA256；无需全局安装开发依赖。Python 3.12+。"""
import hashlib
import json
import tarfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGES = [
    ("fltk-1.4.5.tar.gz", "https://codeload.github.com/fltk/fltk/tar.gz/refs/tags/release-1.4.5", "7715e69ce081fa9ce6da48bb0dd3b07a4cf2cf937813814c04272f36fff593ea"),
    ("sqlite-amalgamation-3540000.zip", "https://www.sqlite.org/2026/sqlite-amalgamation-3540000.zip", "68e913b0fe8ec6b4e5f391c594a28872884e80d6cd042a1e50c957be46b5aa4d"),
]

def main():
    vendor = ROOT / "vendor"
    vendor.mkdir(exist_ok=True)
    for name, url, digest in PACKAGES:
        archive = vendor / name
        if not archive.exists():
            print(f"Downloading {name}")
            with urllib.request.urlopen(url, timeout=90) as response:
                archive.write_bytes(response.read())
        actual = hashlib.sha256(archive.read_bytes()).hexdigest()
        if actual != digest:
            raise RuntimeError(f"SHA256 mismatch: {name}; expected {digest}, actual {actual}")
        if name.endswith(".tar.gz"):
            with tarfile.open(archive) as package:
                package.extractall(vendor, filter="data")
        else:
            with zipfile.ZipFile(archive) as package:
                # ZIP 本身经过校验，仍检查解压路径不越出指定目录。
                for entry in package.infolist():
                    target = (vendor / entry.filename).resolve()
                    if not target.is_relative_to(vendor.resolve()):
                        raise RuntimeError("Archive path escapes vendor directory")
                package.extractall(vendor)
        print(f"Verified {name}: {digest}")
    (vendor / "download-manifest.json").write_text(json.dumps(PACKAGES, ensure_ascii=False, indent=2), encoding="utf-8")

if __name__ == "__main__":
    main()
