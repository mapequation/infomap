"""Agent-usability fixes on the Result / Network surface.

Covers the improvements that keep an agent from stumbling on the immutable
Result API: a physical-node count off the Result, actionable messages for the
higher-order and stale-Result guards, the ``depth`` spelling on ``write_clu``
(matching the readers), a helpful hint for the ``physical_nodes`` legacy
accessor, and host-aware ``write()`` messages that never advertise a format the
Result cannot write.
"""

from __future__ import annotations

import pytest

import infomap
from infomap import Infomap, StaleResultError, run

pytestmark = pytest.mark.fast

_LINKS = [(1, 2), (2, 3), (1, 3), (3, 4), (4, 5), (5, 6), (4, 6)]


def test_num_physical_nodes_first_order_equals_num_nodes():
    result = run(infomap.datasets.two_triangles())
    assert result.num_physical_nodes == result.num_nodes == 6


def test_num_physical_nodes_higher_order_counts_physical_nodes():
    result = run(infomap.datasets.states())
    assert result.have_memory
    assert result.num_nodes == 6  # state nodes
    assert result.num_physical_nodes == 5  # distinct physical nodes


def test_num_physical_nodes_is_eager_and_survives_rerun():
    im = Infomap()
    im.add_links(_LINKS)
    result = im.run()
    im.run()  # bumps the generation; node-level access would now be stale
    assert result.num_physical_nodes == 6  # eager scalar, still valid


def test_physical_nodes_gives_a_result_pointer():
    result = run(infomap.datasets.two_triangles())
    with pytest.raises(AttributeError, match=r"result\.nodes\(states=False\)"):
        result.physical_nodes


def test_higher_order_modules_message_points_at_states_true():
    result = run(infomap.datasets.states())
    with pytest.raises(infomap.InfomapError, match=r"states=True"):
        result.modules(states=False)


def test_stale_result_message_carries_a_remedy():
    im = Infomap()
    im.add_links(_LINKS)
    stale = im.run()
    im.run()
    with pytest.raises(StaleResultError) as excinfo:
        stale.modules()
    message = str(excinfo.value)
    assert "stale Result" in message  # original prefix preserved
    assert "infomap.run" in message and "materialize" in message


def test_write_clu_depth_alias_matches_depth_level(tmp_path):
    result = run(infomap.datasets.nine_triangles(), num_trials=2, seed=123)
    by_depth = tmp_path / "depth.clu"
    by_legacy = tmp_path / "legacy.clu"
    result.write_clu(by_depth, depth=2)
    result.write_clu(by_legacy, depth_level=2)
    assert by_depth.read_bytes() == by_legacy.read_bytes()


def test_write_clu_depth_overrides_depth_level(tmp_path):
    result = run(infomap.datasets.nine_triangles(), num_trials=2, seed=123)
    both = tmp_path / "both.clu"
    only = tmp_path / "only.clu"
    result.write_clu(both, depth=2, depth_level=1)  # depth wins
    result.write_clu(only, depth=2)
    assert both.read_bytes() == only.read_bytes()


def test_result_write_no_extension_message_excludes_net(tmp_path):
    result = run(infomap.datasets.two_triangles())
    with pytest.raises(ValueError) as excinfo:
        result.write(tmp_path / "no_extension")
    message = str(excinfo.value)
    assert ".clu" in message and ".tree" in message
    assert ".net" not in message  # Result cannot write the network serialization


def test_result_write_net_points_to_the_network(tmp_path):
    result = run(infomap.datasets.two_triangles())
    with pytest.raises(NotImplementedError, match="network.write_pajek"):
        result.write(tmp_path / "x.net")
