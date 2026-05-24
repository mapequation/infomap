import subprocess
import sys
import textwrap

import pytest


pytestmark = [pytest.mark.crash]


SMOKE_SCRIPT = """
from infomap import Infomap
import torch

_ = torch.ones((64, 64)) @ torch.ones((64, 64))
im = Infomap("--directed --silent --num-trials 1 --seed 1 --two-level")
for source, target, weight in [
    (0, 1, 1.0),
    (1, 2, 1.0),
    (2, 0, 1.0),
    (3, 4, 1.0),
    (4, 5, 1.0),
    (5, 3, 1.0),
    (2, 3, 0.01),
    (3, 2, 0.01),
]:
    im.add_link(source, target, weight)
im.run()
if not 1.0 < im.codelength < 2.0:
    raise AssertionError(f"unexpected codelength: {im.codelength}")
"""


@pytest.mark.parametrize(
    "prefix",
    [
        pytest.param("import infomap\n", id="infomap-first"),
        pytest.param("import torch\n", id="torch-first"),
    ],
)
def test_torch_and_infomap_run_in_same_process(prefix):
    pytest.importorskip("torch")

    code = prefix + textwrap.dedent(SMOKE_SCRIPT)
    result = subprocess.run(
        [sys.executable, "-c", code],
        check=False,
        capture_output=True,
        text=True,
        timeout=30,
    )

    assert result.returncode == 0, (
        f"subprocess failed with exit code {result.returncode}\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}"
    )
