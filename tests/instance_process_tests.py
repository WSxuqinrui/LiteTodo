"""只启动与终止本测试的隔离进程，验证重复启动及不同数据目录。"""
import argparse
import subprocess
import tempfile
import time
from pathlib import Path

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    binary = parser.parse_args().binary.resolve()
    processes = []
    with tempfile.TemporaryDirectory(prefix='litetodo-instance-test-') as tmp:
        root = Path(tmp)
        try:
            first = subprocess.Popen([str(binary), '--data-dir', str(root / 'one')])
            processes.append(first)
            ready = time.monotonic() + 5
            while not (root / 'one' / 'tasks.sqlite3').exists() and time.monotonic() < ready:
                assert first.poll() is None, 'First instance exited unexpectedly'
                time.sleep(.1)
            assert (root / 'one' / 'tasks.sqlite3').exists(), 'First instance failed to open its database'
            second = subprocess.Popen([str(binary), '--data-dir', str(root / 'one')])
            processes.append(second)
            try:
                result = second.wait(timeout=5)
            except subprocess.TimeoutExpired:
                raise AssertionError('Second instance remained active on the same data directory')
            assert result == 0 and first.poll() is None, 'Duplicate startup failed or stopped owner'
            deadline = time.monotonic() + 3
            while (root / 'one' / '.litetodo-activate').exists() and time.monotonic() < deadline:
                time.sleep(.1)
            assert not (root / 'one' / '.litetodo-activate').exists(), 'Owner did not consume activation request'
            third = subprocess.Popen([str(binary), '--data-dir', str(root / 'two')])
            processes.append(third)
            time.sleep(1)
            assert third.poll() is None and first.poll() is None, 'Independent data directory could not run'
            first.terminate()
            first.wait(timeout=5)
            restarted = subprocess.Popen([str(binary), '--data-dir', str(root / 'one')])
            processes.append(restarted)
            time.sleep(1)
            assert restarted.poll() is None, 'Process exit left a stale lock'
            # 重新启动后再发一次请求，证明新 owner 已取得锁且能够消费请求。
            duplicate = subprocess.Popen([str(binary), '--data-dir', str(root / 'one')])
            processes.append(duplicate)
            assert duplicate.wait(timeout=5) == 0, 'Restarted owner failed to acquire released lock'
            deadline = time.monotonic() + 3
            while (root / 'one' / '.litetodo-activate').exists() and time.monotonic() < deadline:
                time.sleep(.1)
            assert not (root / 'one' / '.litetodo-activate').exists(), 'Restarted owner did not consume request'
            print('PASS duplicate exit, activation consumption, independent directory, lock recovery after termination')
        finally:
            for process in reversed(processes):
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)

if __name__ == '__main__':
    main()
