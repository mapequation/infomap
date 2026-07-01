"""Guard: ``import infomap`` must not eagerly import optional heavy deps.

pandas/numpy link an OpenMP runtime. On macOS, importing them at package-load
time loads a *second* ``libomp`` alongside the wheel-bundled one and aborts
``import infomap`` / the CLI with "OMP: Error #15" in environments (e.g. conda)
that ship their own ``libomp``. pandas is optional -- only the DataFrame
accessors need it -- so it must be imported lazily on first use, never at
import time.

Regression test for #706.
"""

import subprocess
import sys

# Optional, OpenMP-linked deps that ``import infomap`` must not pull in.
_FORBIDDEN_EAGER = ("pandas", "numpy")


def test_import_infomap_does_not_eagerly_import_optional_deps():
    # Run in a fresh interpreter: the pytest process itself already has
    # pandas/numpy loaded (by other tests / plugins), so we cannot inspect
    # ``sys.modules`` in-process.
    code = (
        "import sys, infomap;"
        f"forbidden = {_FORBIDDEN_EAGER!r};"
        "print(','.join(m for m in forbidden if m in sys.modules))"
    )
    result = subprocess.run(
        [sys.executable, "-c", code],
        capture_output=True,
        text=True,
        check=True,
    )
    leaked = [name for name in result.stdout.strip().split(",") if name]
    assert leaked == [], (
        f"`import infomap` eagerly imported {leaked}; on macOS these load a "
        "second OpenMP runtime and abort with 'OMP: Error #15'. Keep them lazy "
        "(resolve pandas via infomap._optional.get_pandas() inside the methods "
        "that need it, not at module scope)."
    )
