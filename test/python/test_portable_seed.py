"""A seed reaches the same tree with every standard library (#342, #1252).

Pins the trees that ``test/cpp/test_random.cpp`` pins, so the MSVC-built wheel
is held to the same trees as the libstdc++ and libc++ builds. The Windows
Python CI leg runs this file.
"""

from __future__ import annotations

import infomap
import pytest

pytestmark = pytest.mark.fast

NETWORK = "test/fixtures/networks/planted_partition.net"

# seed: (codelength, levels, top modules, partition fingerprint)
EXPECTED = {
    1: (6.4888344247633523, 2, 9, 6013868599683882252),
    2: (6.4056182367171912, 2, 7, 10620822623069457366),
    3: (6.5174214181909962, 2, 8, 5123504666681688741),
}


def _fingerprint(modules):
    """FNV-1a over the canonical top-level partition, as in test_random.cpp."""
    groups = {}
    for node_id, module_id in modules.items():
        groups.setdefault(module_id, []).append(node_id)
    mask = (1 << 64) - 1
    value = 14695981039346656037
    for group in sorted(sorted(nodes) for nodes in groups.values()):
        for node_id in group:
            value = ((value ^ node_id) * 1099511628211) & mask
        value = ((value ^ 0xFFFFFFFF) * 1099511628211) & mask
    return value


@pytest.mark.parametrize("seed", sorted(EXPECTED))
def test_seed_reaches_the_same_tree_on_every_standard_library(seed):
    codelength, num_levels, num_top_modules, fingerprint = EXPECTED[seed]
    im = infomap.Infomap(silent=True, num_trials=1, seed=seed)
    im.read_file(NETWORK)
    im.run()
    assert im.codelength == pytest.approx(codelength, abs=1e-10)
    assert im.num_levels == num_levels
    assert im.num_top_modules == num_top_modules
    assert _fingerprint(im.get_modules(depth_level=1)) == fingerprint
