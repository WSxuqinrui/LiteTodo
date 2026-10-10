"""封装发布包并执行 5,000,000 字节门槛，所有随包文件都计入。"""
import argparse
import hashlib
import json
import shutil
import zipfile
import tempfile
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 5_000_000

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--platform", required=True, choices=["windows-x64", "macos-arm64", "macos-x64", "linux-x64"])
    args = parser.parse_args()
    dist = ROOT / "dist"
    dist.mkdir(parents=True, exist_ok=True)
    release = dist / args.platform
    # 新建空目录，不以已运行过的发布目录为输入；旧目录可包含用户便携数据。
    with tempfile.TemporaryDirectory(prefix=args.platform + "-stage-", dir=dist) as staging:
        if not Path(staging).resolve().is_relative_to(dist.resolve()):
            raise RuntimeError("Staging path must stay inside the release directory")
        destination = Path(staging) / args.platform
        destination.mkdir()
        build_release(args, destination, release, dist)

def build_release(args, destination, release, dist):
    name = args.binary.name
    target = destination / name
    if args.binary.is_dir():
        shutil.copytree(args.binary, target, dirs_exist_ok=True)
    else:
        shutil.copy2(args.binary, target)
    shutil.copy2(ROOT / "README.md", destination / "使用说明.md")
    shutil.copy2(ROOT / "THIRD_PARTY_NOTICES.md", destination / "第三方声明.md")
    docs = destination / "docs"
    docs.mkdir(exist_ok=True)
    shutil.copy2(ROOT / "docs/interaction-reference.md", docs / "interaction-reference.md")
    shutil.copy2(ROOT / "docs/revision-design.md", docs / "revision-design.md")
    if (ROOT / "docs/development.md").is_file():
        shutil.copy2(ROOT / "docs/development.md", docs / "development.md")
    # 使用说明的本地图片一起发布，免安装包离线查看也不会出现失效引用。
    for relative in ("assets/app.png", "docs/screenshots/main.png", "docs/screenshots/editor.png", "docs/screenshots/console.png"):
        source = ROOT / relative
        if source.is_file():
            output = destination / relative
            output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, output)
    licenses = destination / "licenses"
    licenses.mkdir(exist_ok=True)
    shutil.copy2(ROOT / "vendor/fltk-release-1.4.5/COPYING", licenses / "FLTK-LICENSE.txt")
    # 静态编译器运行库的授权文件随同实验包保留。
    for file in (ROOT / "docs/licenses").glob("*"):
        if file.is_file():
            shutil.copy2(file, licenses / file.name)
    if args.platform == "windows-x64":
        (destination / "便携数据模式.cmd").write_bytes(b'@echo off\r\npushd "%~dp0"\r\nstart "" "%~dp0LiteTodo.exe" --portable\r\npopd\r\n')
    files = sorted(path for path in destination.rglob("*") if path.is_file() and path.name != "SIZE.json")
    records = [{"file": p.relative_to(destination).as_posix(), "bytes": p.stat().st_size,
                "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]
    report = {"platform": args.platform, "limit_bytes": LIMIT, "payload_bytes": sum(x["bytes"] for x in records),
              "files": records, "runtime_dependencies": "Must additionally inspect imports; file size alone does not prove portable deployment"}
    size_file = destination / "SIZE.json"
    size_file.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    total = sum(p.stat().st_size for p in destination.rglob("*") if p.is_file())
    if total > LIMIT:
        raise RuntimeError(f"Package exceeds budget: {total:,} > {LIMIT:,} bytes")
    archive = dist / f"LiteTodo-0.4.1-{args.platform}.zip"
    temporary_archive = destination.parent / archive.name
    with zipfile.ZipFile(temporary_archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for file in sorted(destination.rglob("*")):
            if file.is_file():
                package.write(file, file.relative_to(destination))
    # 已有目录改名留档，里面可能有便携数据，不能删除或混入新 ZIP。
    previous = dist / (args.platform + "-previous-" + uuid.uuid4().hex[:12])
    if release.exists():
        release.rename(previous)
    try:
        destination.rename(release)
    except Exception:
        if previous.exists() and not release.exists():
            previous.rename(release)
        raise
    temporary_archive.replace(archive)
    print(json.dumps({"unpacked_total_bytes": total, "zip_bytes": archive.stat().st_size,
                      "archive": str(archive), "limit_bytes": LIMIT}, ensure_ascii=False, indent=2))

if __name__ == "__main__":
    main()
