"""File writers on Result and Network (#760).

The blessed path can write the mapequation.org tool files: the
result-artifact writers (tree/ftree/clu/Newick/JSON/CSV) live on
:class:`Result`, the network-describing writers (pajek/state network) on
:class:`Network`, and :class:`Infomap` keeps its full historical set through
2.x. Result writers are generation-guarded like all node-level access.
"""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from infomap import Network, Options, StaleResultError, run


pytestmark = pytest.mark.fast

_LINKS = [(1, 2), (2, 3), (1, 3), (3, 4), (4, 5), (5, 6), (4, 6)]


@pytest.fixture
def run_result(make_infomap):
    im = make_infomap(two_level=True)
    im.add_links(_LINKS)
    return im, im.run()


@pytest.mark.parametrize(
    ("result_writer", "legacy_writer"),
    [
        ("write_tree", "write_tree"),
        ("write_flow_tree", "write_flow_tree"),
        ("write_clu", "write_clu"),
        ("write_newick", "write_newick"),
        ("write_json", "write_json"),
        ("write_csv", "write_csv"),
    ],
)
def test_result_writer_matches_legacy_infomap_writer(
    run_result, tmp_path, result_writer, legacy_writer
):
    im, result = run_result
    from_result = tmp_path / "from_result.out"
    from_legacy = tmp_path / "from_legacy.out"

    getattr(result, result_writer)(from_result)
    getattr(im, legacy_writer)(from_legacy)

    assert from_result.read_bytes() == from_legacy.read_bytes()


def test_result_write_dispatches_on_extension(run_result, tmp_path):
    _, result = run_result

    result.write(tmp_path / "partition.clu")
    result.write(tmp_path / "partition.ftree")

    assert (tmp_path / "partition.clu").stat().st_size > 0
    assert (tmp_path / "partition.ftree").stat().st_size > 0


def test_result_write_rejects_network_formats(run_result, tmp_path):
    # .net maps to the pajek writer, which describes the input network and
    # lives on Network/Infomap, not on the result.
    _, result = run_result

    with pytest.raises(NotImplementedError, match="pajek"):
        result.write(tmp_path / "network.net")


def test_stale_result_writers_raise(run_result, tmp_path):
    im, result = run_result
    im.run()  # rebuilds the C++ result tree; `result` is now stale

    with pytest.raises(StaleResultError, match="stale Result"):
        result.write_tree(tmp_path / "stale.tree")


def test_functional_run_result_writes_without_the_stateful_class(tmp_path):
    result = run(_LINKS, two_level=True, seed=123)

    result.write_tree(tmp_path / "functional.tree")

    content = (tmp_path / "functional.tree").read_text()
    assert content.startswith("#")
    assert " 1 " in content or ":" in content


def test_network_run_result_writes(tmp_path):
    net = Network().add_links(_LINKS)
    result = net.run(options=Options(two_level=True, seed=123))

    result.write_clu(tmp_path / "network.clu")

    assert (tmp_path / "network.clu").stat().st_size > 0


def _data_lines(path: Path) -> list[str]:
    # The header comments embed the engine's construction flags, which differ
    # between a Network engine and an Infomap engine; the payload must not.
    return [
        line for line in path.read_text().splitlines() if not line.startswith("#")
    ]


def test_network_writers_match_legacy_infomap_writers(make_infomap, tmp_path):
    net = Network().add_links(_LINKS)
    im = make_infomap()
    im.add_links(_LINKS)

    net.write_pajek(tmp_path / "net.net")
    im.write_pajek(tmp_path / "im.net")
    net.write_state_network(tmp_path / "net.states")
    im.write_state_network(tmp_path / "im.states")

    assert _data_lines(tmp_path / "net.net") == _data_lines(tmp_path / "im.net")
    assert _data_lines(tmp_path / "net.states") == _data_lines(tmp_path / "im.states")


def test_result_writers_accept_path_objects(run_result, tmp_path):
    _, result = run_result
    target: Path = tmp_path / "pathlib.tree"

    result.write_tree(target)

    assert target.stat().st_size > 0


def test_result_writers_accept_generic_pathlike(run_result, tmp_path, fspath_only):
    _, result = run_result
    target: Path = tmp_path / "generic.tree"

    result.write_tree(fspath_only(target))

    assert target.stat().st_size > 0


def test_result_writers_accept_bytes_paths(run_result, tmp_path):
    # The readers decode with os.fsdecode (#809), which also accepts a bytes
    # path; the writers must mirror it so the same path value works on both
    # sides of a run instead of leaking a raw SWIG TypeError.
    _, result = run_result
    target: Path = tmp_path / "bytes.tree"

    result.write_tree(os.fsencode(target))

    assert target.stat().st_size > 0


def test_result_generic_write_accepts_bytes_paths(run_result, tmp_path):
    _, result = run_result
    target: Path = tmp_path / "generic-bytes.tree"

    result.write(os.fsencode(target))

    assert target.stat().st_size > 0


def test_network_writers_accept_bytes_paths(tmp_path):
    net = Network()
    net.add_links(_LINKS)
    target: Path = tmp_path / "bytes.net"

    net.write_pajek(os.fsencode(target))

    assert target.stat().st_size > 0


def test_result_stays_slots_based():
    # The writer mixin must not reintroduce a __dict__ on the slots-based,
    # immutable Result.
    result = run(_LINKS, two_level=True, seed=123)

    assert not hasattr(result, "__dict__")
    with pytest.raises(AttributeError, match="immutable"):
        result.write_tree = None
