from collections import defaultdict
import math
import re

import networkx as nx
import pytest


pytestmark = pytest.mark.fast


def test_precomputed_requirements(make_infomap, example_network_path):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(example_network_path("twotriangles.net")))
    with pytest.raises(
        RuntimeError,
        match=re.escape(
            "Missing node flow in input data. Should be passed as a third field under a *Vertices section."
        ),
    ):
        im.run()


def test_precomputed_states_requirements(make_infomap, example_network_path):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(example_network_path("states.net")))
    with pytest.raises(
        RuntimeError,
        match=re.escape(
            "Missing node flow in input data. Should be passed as a third field under a *States section."
        ),
    ):
        im.run()


def test_precomputed_from_file(make_infomap, fixture_path, output_dir, assert_pajek_matches_fixture):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(fixture_path("twotriangles_flow.net")))
    im.run()
    output_path = output_dir / "twotriangles_flow_output.net"
    im.write_pajek(str(output_path), flow=True)
    assert_pajek_matches_fixture(output_path, fixture_path("twotriangles_flow_output.net"))


def test_precomputed_states_from_file(make_infomap, fixture_path, output_dir, assert_pajek_matches_fixture):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(fixture_path("states_flow.net")))
    im.run()
    output_path = output_dir / "states_flow_output.net"
    im.write_pajek(str(output_path), flow=True)
    assert_pajek_matches_fixture(output_path, fixture_path("states_flow_output.net"))


def test_recorded_teleportation_codelength_is_stable_across_trials(make_infomap):
    graph = nx.DiGraph()
    graph.add_edges_from([(1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 6), (6, 4)])

    def canonical_modules(modules):
        groups = defaultdict(list)
        for node_id, module_id in modules.items():
            groups[module_id].append(node_id)
        return sorted(sorted(group) for group in groups.values())

    def run(num_trials):
        im = make_infomap(recorded_teleportation=True, num_trials=num_trials)
        im.add_networkx_graph(graph)
        im.run()
        return im

    one_trial = run(1)
    two_trials = run(2)

    assert canonical_modules(one_trial.get_modules()) == canonical_modules(two_trials.get_modules())
    assert math.isclose(one_trial.codelength, two_trials.codelength)
    assert math.isclose(one_trial.index_codelength, two_trials.index_codelength)
