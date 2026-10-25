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
from infomap import Infomap, InfomapError, StaleResultError, datasets, run

pytestmark = pytest.mark.fast

_LINKS = [(1, 2), (2, 3), (1, 3), (3, 4), (4, 5), (5, 6), (4, 6)]


def test_num_physical_nodes_first_order_equals_num_nodes():
    result = run(datasets.two_triangles())
    assert result.num_physical_nodes == result.num_nodes == 6


def test_num_physical_nodes_higher_order_counts_physical_nodes():
    result = run(datasets.states())
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
    result = run(datasets.two_triangles())
    with pytest.raises(AttributeError, match=r"result\.nodes\(states=False\)"):
        result.physical_nodes


def test_higher_order_modules_message_points_at_states_true():
    result = run(datasets.states())
    with pytest.raises(InfomapError, match=r"states=True"):
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
    assert "materialize" in message
    # This used to also require the message to name infomap.run() as the
    # staleness-free alternative. It is not one: run(network) re-runs that same
    # network in place and bumps the generation, so following the advice
    # reproduces this error. The remedy has to be one that actually works.
    assert "infomap.run()" not in message
    assert "its own engine" in message


def test_write_clu_level_aliases_match_and_announce_themselves(tmp_path):
    from infomap._options import LEGACY_SURFACE_WARNING

    result = run(datasets.nine_triangles(), num_trials=2, seed=123)
    by_level = tmp_path / "level.clu"
    by_depth = tmp_path / "depth.clu"
    by_legacy = tmp_path / "legacy.clu"
    positional = tmp_path / "positional.clu"
    result.write_clu(by_level, level=2)
    result.write_clu(positional, False, 2)  # the level stays the third positional
    with pytest.warns(LEGACY_SURFACE_WARNING, match="'depth' is deprecated"):
        result.write_clu(by_depth, depth=2)
    with pytest.warns(LEGACY_SURFACE_WARNING, match="'depth_level' is deprecated"):
        result.write_clu(by_legacy, depth_level=2)
    assert by_level.read_bytes() == by_depth.read_bytes()
    assert by_level.read_bytes() == by_legacy.read_bytes()
    assert by_level.read_bytes() == positional.read_bytes()


def test_infomap_write_clu_alias_warning_names_no_receiver(tmp_path):
    # The same writer mixin serves the stateful Infomap, so the message must
    # not claim the caller used Result.
    from infomap import Infomap
    from infomap._options import LEGACY_SURFACE_WARNING

    im = Infomap(num_trials=1, seed=1)
    im.add_links([(0, 1), (1, 2), (2, 0)])
    im.run()
    with pytest.warns(LEGACY_SURFACE_WARNING) as records:
        im.write_clu(tmp_path / "im.clu", depth=1)
    messages = [str(r.message) for r in records if "'depth'" in str(r.message)]
    assert messages and "on write_clu()" in messages[0]
    assert "Result." not in messages[0]


def test_write_clu_conflicting_level_spellings_raise(tmp_path):
    # Two spellings with different values used to be resolved silently (depth
    # won); picking one would hide a migration mistake, so it is an error, as
    # on to_dataframe.
    result = run(datasets.nine_triangles(), num_trials=2, seed=123)
    with pytest.raises(ValueError, match="Conflicting values for the tree level"):
        result.write_clu(tmp_path / "both.clu", level=2, depth_level=1)


def test_result_write_no_extension_message_excludes_net(tmp_path):
    result = run(datasets.two_triangles())
    with pytest.raises(ValueError) as excinfo:
        result.write(tmp_path / "no_extension")
    message = str(excinfo.value)
    assert ".clu" in message and ".tree" in message
    assert ".net" not in message  # Result cannot write the network serialization


def test_result_write_net_points_to_the_network(tmp_path):
    result = run(datasets.two_triangles())
    with pytest.raises(NotImplementedError, match="network.write_pajek"):
        result.write(tmp_path / "x.net")
