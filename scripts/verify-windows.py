"""隔离 Windows UI 检查与资源采样；只操作自身启动的验证进程。"""
import ctypes
import argparse
import json
import subprocess
import time
import uuid
from pathlib import Path
from ctypes import wintypes

ROOT = Path(__file__).resolve().parents[1]

class MemoryCounters(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD),
                ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t), ("PrivateUsage", ctypes.c_size_t)]

def sample(handle):
    counters = MemoryCounters()
    counters.cb = ctypes.sizeof(counters)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(MemoryCounters), wintypes.DWORD]
    if not psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
        raise ctypes.WinError(ctypes.get_last_error())
    creation, exit_time, kernel, user = (wintypes.FILETIME() for _ in range(4))
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    if not kernel32.GetProcessTimes(handle, ctypes.byref(creation), ctypes.byref(exit_time), ctypes.byref(kernel), ctypes.byref(user)):
        raise ctypes.WinError(ctypes.get_last_error())
    def seconds(value):
        return ((value.dwHighDateTime << 32) | value.dwLowDateTime) / 10_000_000
    return {"working_set_bytes": counters.WorkingSetSize, "private_bytes": counters.PrivateUsage,
            "cpu_seconds": seconds(kernel) + seconds(user), "monotonic": time.monotonic()}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, default=ROOT / "build-windows/LiteTodo.exe")
    binary = parser.parse_args().binary.resolve()
    data = ROOT / "verification" / ("最终中文验证-" + uuid.uuid4().hex[:8])
    data.mkdir(parents=True)
    with (data / "process.log").open("wb") as log:
        process = subprocess.Popen([str(binary), "--ui-smoke", "--data-dir", str(data)], stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 30
            while not (data / "ui-smoke.txt").exists():
                if process.poll() is not None:
                    raise RuntimeError(f"UI exited before result: {process.returncode}")
                if time.monotonic() > deadline:
                    raise TimeoutError("UI result timed out")
                time.sleep(0.2)
            result = (data / "ui-smoke.txt").read_text(encoding="utf-8")
            if not result.startswith("UI callbacks: PASS"):
                raise RuntimeError(result)
            before = sample(int(process._handle))
            time.sleep(8)
            after = sample(int(process._handle))
            resource = {"before": before, "after": after, "idle_cpu_one_core_percent":
                        100 * (after["cpu_seconds"] - before["cpu_seconds"]) / (after["monotonic"] - before["monotonic"]),
                        "sampling": "main window after synthetic task/settings flows; initial font/render costs excluded"}
            process.wait(timeout=45)
            if process.returncode != 0 or not (data / "ui-exit.txt").exists():
                raise RuntimeError(f"Unclean exit: {process.returncode}")
            result = (data / "ui-smoke.txt").read_text(encoding="utf-8")
            for contract in ("Native round window region: PASS", "Inline menu start/urgent/shelve/resume/complete/delete/cancel: PASS",
                             "Round region after ball/main hide/show, move and 150 percent scale: PASS",
                             "Main-page subtask check/reopen, board/narrow and draft protection: PASS",
                             "Multiline padding and soft scrollbar drag: PASS",
                             "Free-ball hover and draft preservation: PASS", "Fallback acknowledgement and snooze: PASS"):
                if contract not in result:
                    raise RuntimeError(f"Missing regression result: {contract}")
            resource["clean_exit"] = True
            native_state = (data / "native-window-state.txt").read_text(encoding="utf-8")
            if "alpha 204" not in native_state or "Ball topmost after hide/show and scale: PASS" not in native_state:
                raise RuntimeError("Missing native opacity/topmost checks")
            (data / "resources.json").write_text(json.dumps(resource, ensure_ascii=False, indent=2), encoding="utf-8")
            # 这里只将应用自身的真实控件渲染捕获转存 PNG，不改变像素内容。
            from PIL import Image
            for capture in data.glob("*-render.ppm"):
                Image.open(capture).save(ROOT / "verification" / (capture.stem + ".png"))
            (ROOT / "verification/latest-run.txt").write_text(str(data), encoding="utf-8")
            print(result)
            print(native_state)
            print(json.dumps(resource, ensure_ascii=False, indent=2))
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)

if __name__ == "__main__":
    main()
