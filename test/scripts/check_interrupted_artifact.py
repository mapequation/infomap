#!/usr/bin/env python3
"""An interrupted run must leave an artifact that cannot pass for a complete one.

updateBestResult rewrites the tree after every improving trial, so a run killed partway
leaves a complete, well-formed best-of-k file. Its header recorded the requested
--num-trials and an elapsed time, but never how many trials had actually run, so a
pipeline collecting *.tree could not tell a 500-trial search from an aborted one. SIGTERM
-- SLURM time limits, `timeout`, preemption, the OOM killer -- was not handled at all
(#906).

Checks, for both SIGINT and SIGTERM:
  * the process shuts down through the cancellation seam rather than dying on the signal
  * the artifact's header reports fewer trials than were asked for
"""

import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REQUESTED_TRIALS = 5000


def _write_network(path: Path) -> None:
    """A network big enough that 5000 trials cannot finish while we wait."""
    lines = ["*Vertices 400"]
    lines += [f'{i} "n{i}"' for i in range(1, 401)]
    lines.append("*Edges")
    for i in range(1, 401):
        for offset in (1, 2, 7):
            target = (i + offset - 1) % 400 + 1
            if target != i:
                lines.append(f"{i} {target}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _trials_in_header(tree_path: Path) -> tuple[int, int] | None:
    for line in tree_path.read_text(encoding="utf-8").splitlines():
        if line.startswith("# trials "):
            parts = line.split()
            return int(parts[2]), int(parts[4])
        if not line.startswith("#"):
            break
    return None


def _run_and_signal(binary: str, sig: int, work: Path) -> tuple[int, Path]:
    out_dir = work / f"out_{sig}"
    out_dir.mkdir()
    process = subprocess.Popen(
        [
            binary,
            str(work / "net.net"),
            str(out_dir),
            "--num-trials",
            str(REQUESTED_TRIALS),
            "--seed",
            "1",
            "--silent",
            "--tree",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    tree = out_dir / "net.tree"
    # Wait until at least one trial has been published, so there is an artifact to judge.
    deadline = time.time() + 60
    while time.time() < deadline and not tree.exists():
        if process.poll() is not None:
            break
        time.sleep(0.05)
    # The run can finish in the race between the poll above and the signal below; a
    # ProcessLookupError there would make this test flaky rather than informative.
    if process.poll() is None:
        try:
            process.send_signal(sig)
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=60)
    except subprocess.TimeoutExpired:
        process.kill()
        raise SystemExit(f"signal {sig} did not stop the run within 60 s") from None
    return process.returncode, tree


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: check_interrupted_artifact.py <infomap-binary>", file=sys.stderr)
        return 2
    binary = argv[1]

    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        _write_network(work / "net.net")

        for sig in (signal.SIGINT, signal.SIGTERM):
            code, tree = _run_and_signal(binary, sig, work)

            if code == -int(sig):
                print(
                    f"signal {sig} killed the process outright (rc {code}); it has to go "
                    "through the cancellation seam so the run can shut down cleanly",
                    file=sys.stderr,
                )
                return 1
            if code == 0:
                print(f"signal {sig} was ignored: the run exited 0", file=sys.stderr)
                return 1

            if not tree.exists():
                # Nothing published yet is a legitimate outcome on a fast machine; there
                # is then no artifact that could be mistaken for a complete one.
                print(f"signal {sig}: no artifact was published, nothing to confuse")
                continue

            header = _trials_in_header(tree)
            if header is None:
                print(
                    f"signal {sig}: the artifact's header has no '# trials N of M' line, "
                    "so it cannot be told apart from a complete run",
                    file=sys.stderr,
                )
                return 1
            ran, requested = header
            if requested != REQUESTED_TRIALS:
                print(
                    f"signal {sig}: header requested {requested}, expected {REQUESTED_TRIALS}",
                    file=sys.stderr,
                )
                return 1
            if ran >= requested:
                print(
                    f"signal {sig}: header claims {ran} of {requested} trials ran, but the "
                    "run was interrupted",
                    file=sys.stderr,
                )
                return 1
            print(
                f"signal {sig}: rc {code}, artifact reports {ran} of {requested} trials"
            )

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
