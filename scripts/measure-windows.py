"""隔离资源采样：不执行 UI 渲染压力检查，只启动自身的测试副本。"""
import argparse
import importlib.util
import json
import shutil
import subprocess
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ui_probe", ROOT / "scripts/verify-windows.py")
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, default=ROOT / "dist/windows-x64/LiteTodo.exe")
    parser.add_argument("--fixture", type=Path, required=True, help="已关闭数据库的一致备份，复制到新目录后采样")
    args = parser.parse_args()
    data = ROOT / "verification" / ("普通启动采样-" + uuid.uuid4().hex[:8])
    data.mkdir(parents=True)
    shutil.copy2(args.fixture, data / "tasks.sqlite3")
    process = subprocess.Popen([str(args.binary.resolve()), "--data-dir", str(data)])
    try:
        time.sleep(10)
        if process.poll() is not None:
            raise RuntimeError("Resource probe exited before measurement")
        before = probe.sample(int(process._handle))
        time.sleep(8)
        after = probe.sample(int(process._handle))
        result = {"before": before, "after": after,
                  "idle_cpu_one_core_percent": 100 * (after["cpu_seconds"] - before["cpu_seconds"]) / (after["monotonic"] - before["monotonic"]),
                  "sampling": "normal start with copied synthetic tasks; ten-second settle; no render stress; one eight-second observation",
                  "shutdown": "Only this isolated probe process is terminated; clean exit was checked by ui-smoke separately"}
        (data / "resources.json").write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
        (ROOT / "verification/latest-resource-run.txt").write_text(str(data),encoding="utf-8")
        print(json.dumps(result,ensure_ascii=False,indent=2))
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)

if __name__ == "__main__":
    main()
