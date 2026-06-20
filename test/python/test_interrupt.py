"""Cooperative interruption of a running Infomap (issue #412).

The C++ seam is covered by the native tests; here we verify the Python host
bridge end to end: a real SIGINT delivered to a process running ``Infomap.run()``
surfaces as ``KeyboardInterrupt`` promptly, instead of being queued until the
(otherwise non-interruptible) C extension call returns.

Signal delivery is POSIX-specific, so the delivery test is skipped on Windows;
the bridge itself still compiles and runs there.
"""

import signal
import subprocess
import sys
import time

import pytest

pytestmark = pytest.mark.fast


# Builds a network large enough that the run is still going when the signal
# arrives, then prints READY and runs many trials.
_CHILD = """
import sys, infomap
im = infomap.Infomap("--silent --num-trials 1000 --no-file-output --seed 1")
clusters = 1500
for c in range(clusters):
    base = c * 4
    nodes = [base + i for i in range(4)]
    for i in range(4):
        for j in range(i + 1, 4):
            im.add_link(nodes[i], nodes[j])
    if c > 0:
        im.add_link(base, base - 4)
print("READY", flush=True)
try:
    im.run()
    print("COMPLETED")
except KeyboardInterrupt:
    print("KEYBOARDINTERRUPT")
    sys.exit(42)
except BaseException as exc:  # noqa: BLE001
    print("OTHER", type(exc).__name__)
    sys.exit(43)
"""


@pytest.mark.skipif(sys.platform == "win32", reason="POSIX SIGINT delivery")
def test_sigint_raises_keyboardinterrupt_during_run():
    proc = subprocess.Popen(
        [sys.executable, "-c", _CHILD],
        stdout=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    try:
        assert proc.stdout.readline().strip() == "READY"
        time.sleep(1.0)  # let the run get well underway
        signaled_at = time.time()
        proc.send_signal(signal.SIGINT)
        out, _ = proc.communicate(timeout=30)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.communicate()

    assert "KEYBOARDINTERRUPT" in out, out
    assert proc.returncode == 42, out
    # The run should stop promptly after the signal, not run all 1000 trials.
    assert time.time() - signaled_at < 15
