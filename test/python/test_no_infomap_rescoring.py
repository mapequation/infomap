"""Re-scoring a partition returns the codelength the partition actually has.

``--no-infomap`` with a supplied partition used to overwrite the codelength the
partition-input path had just established with a fresh whole-tree recompute. For a flat
partition -- a ``.clu``, a two-level ``.tree``, or any tree under ``--two-level`` -- that
recompute summed module enter/exit flow the cluster-id path never establishes, and on
bipartite input those values are negative. A run then reported a codelength built from
negative probabilities, lower than the one it had printed for the same tree (#957).

Asserted as a round trip: scoring a partition Infomap just produced must return the number
Infomap just reported for it.
"""

from __future__ import annotations

import pytest
from infomap import Infomap

pytestmark = pytest.mark.fast


def _rescore(network_path: str, modules: dict[int, int], **options) -> float:
    im = Infomap(num_trials=1, silent=True, no_infomap=True, **options)
    im.read_file(network_path)
    im.run(initial_partition=modules)
    return im.codelength


def test_bipartite_regularized_partition_rescores_to_the_same_codelength(
    example_network_path,
):
    # --regularized turns on recorded teleportation, which is what makes the module
    # enter/exit flow of a bipartite network land negative in the recompute.
    network = str(example_network_path("bipartite.net"))
    options = {"two_level": True, "regularized": True, "seed": 7}

    im = Infomap(num_trials=1, silent=True, **options)
    im.read_file(network)
    result = im.run()
    modules = result.modules()

    # Not a trivial partition, or the equality below could hold for a partition with no
    # module boundaries to get the enter/exit flow of wrong.
    assert len(set(modules.values())) > 1
    assert result.codelength > 0.0

    assert _rescore(network, modules, **options) == pytest.approx(result.codelength)


def test_ordinary_network_partition_rescores_to_the_same_codelength(
    example_network_path,
):
    # The same round trip on an input that never had the defect, so a regression that
    # breaks re-scoring in general cannot hide behind the bipartite case above.
    network = str(example_network_path("ninetriangles.net"))
    options = {"two_level": True, "seed": 7}

    im = Infomap(num_trials=1, silent=True, **options)
    im.read_file(network)
    result = im.run()
    modules = result.modules()

    assert len(set(modules.values())) > 1

    assert _rescore(network, modules, **options) == pytest.approx(result.codelength)


def test_no_infomap_without_a_partition_still_reports_one_level(example_network_path):
    # The recompute is what scores the trivial tree when nothing supplied a partition, so
    # that branch has to survive: every node in one module is the one-level codelength.
    im = Infomap(num_trials=1, silent=True, no_infomap=True)
    im.read_file(str(example_network_path("ninetriangles.net")))
    result = im.run()

    assert result.codelength == pytest.approx(im.one_level_codelength)
