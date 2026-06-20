"""Cooperative interruption of a running Infomap (issue #412).

The C++ seam is covered by the native tests; here we verify the Python host
bridges end to end with a REAL SIGINT delivered to a subprocess:

- the OO API (``Infomap().run()``) surfaces ``KeyboardInterrupt`` mid-run,
  instead of queueing it until the (otherwise non-interruptible) C call returns;
- the pip console-script entry (``infomap:main``) exits 130 with "Interrupted.".

Signal delivery is POSIX-specific, so these are skipped on Windows; the bridges
themselves still compile and run there.

Robustness: the child runs an effectively unbounded number of trials, so it
cannot finish before the signal — a "completed" outcome therefore means the
interrupt was IGNORED (a real failure), never just "the machine was fast".
A broken interrupt instead shows up as the 30s communicate() timeout.
"""

import signal
import subprocess
import sys
import time

import pytest

pytestmark = pytest.mark.fast

# Large trial cap so the run is still going when the signal lands; a working
# interrupt stops it in well under a second regardless of the cap.
_TRIALS = "100000"


def _write_clustered_network(path, clusters=1500):
    """A network of `clusters` weakly-linked 4-cliques (link-list format)."""
    lines = []
    for c in range(clusters):
        base = c * 4 + 1
        nodes = [base + i for i in range(4)]
        for i in range(4):
            for j in range(i + 1, 4):
                lines.append(f"{nodes[i]} {nodes[j]}")
        if c > 0:
            lines.append(f"{base} {base - 4}")
    path.write_text("\n".join(lines) + "\n")


# Reads the network file in argv[1], prints READY, then runs. Exit codes:
# 42 = KeyboardInterrupt (expected), 0 = completed (interrupt ignored), 43 = other.
_OO_CHILD = """
import sys, infomap
im = infomap.Infomap("--silent --num-trials %s --no-file-output --seed 1")
im.read_file(sys.argv[1])
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
""" % _TRIALS

# Drives the pip console-script entry (infomap:main) without needing it on PATH.
_CLI_CHILD = "import sys; from infomap import main; sys.exit(main())"


def _run_until_ready_then_sigint(argv, *, want_ready):
    """Launch argv, optionally wait for a READY line, send SIGINT ~1s in, return
    (returncode, stdout, stderr, seconds_after_signal)."""
    proc = subprocess.Popen(
        argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    out = ""
    try:
        if want_ready:
            assert proc.stdout.readline().strip() == "READY"
        else:
            time.sleep(0.3)  # let process startup + network read finish
        time.sleep(1.0)  # let the run get well underway
        signaled_at = time.time()
        proc.send_signal(signal.SIGINT)
        out, err = proc.communicate(timeout=30)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.communicate()
    return proc.returncode, out, err, time.time() - signaled_at


@pytest.mark.skipif(sys.platform == "win32", reason="POSIX SIGINT delivery")
def test_sigint_raises_keyboardinterrupt_during_run(tmp_path):
    net = tmp_path / "net.txt"
    _write_clustered_network(net)

    code, out, _err, elapsed = _run_until_ready_then_sigint(
        [sys.executable, "-c", _OO_CHILD, str(net)], want_ready=True
    )

    assert code != 0, f"run completed without raising — interrupt was ignored ({out!r})"
    assert code != 43, f"run raised the wrong exception type ({out!r})"
    assert code == 42, (code, out)
    assert "KEYBOARDINTERRUPT" in out, out
    assert elapsed < 20, f"took {elapsed:.1f}s to unwind after the signal"


@pytest.mark.skipif(sys.platform == "win32", reason="POSIX SIGINT delivery")
def test_cli_main_exits_130_on_sigint(tmp_path):
    net = tmp_path / "net.txt"
    _write_clustered_network(net)

    code, _out, err, elapsed = _run_until_ready_then_sigint(
        [sys.executable, "-c", _CLI_CHILD, str(net), str(tmp_path / "out"), "--silent", "-N", _TRIALS, "--seed", "1"],
        want_ready=False,
    )

    # main() catches the propagated KeyboardInterrupt and exits like the native
    # CLI: a clean message and 130 (128 + SIGINT), not a hard signal death (-2)
    # and not 0 (which would mean the interrupt was ignored).
    assert code == 130, f"expected 130 (clean interrupt), got {code}; stderr={err!r}"
    assert "Interrupted." in err, err
    assert elapsed < 20, f"took {elapsed:.1f}s to unwind after the signal"
