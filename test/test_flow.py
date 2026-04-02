from infomap import Infomap
from collections import defaultdict
import math
import networkx as nx
import re
from pathlib import Path
import filecmp
import pytest


def test_precomputed_requirements():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("examples/networks/twotriangles.net")
    with pytest.raises(RuntimeError, match=re.escape("Missing node flow in input data. Should be passed as a third field under a *Vertices section.")):
        im.run()


def test_precomputed_states_requirements():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("examples/networks/states.net")
    with pytest.raises(RuntimeError, match=re.escape("Missing node flow in input data. Should be passed as a third field under a *States section.")):
        im.run()


def test_precomputed_from_file():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("test/data/twotriangles_flow.net")
    im.run()
    output_path = Path("test/data/output/twotriangles_flow_output.net")
    output_path.parent.mkdir(exist_ok=True)
    im.write_pajek(str(output_path), flow=True)
    assert (filecmp.cmp(output_path, "test/data/twotriangles_flow_output.net"))


def test_precomputed_states_from_file():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("test/data/states_flow.net")
    im.run()
    output_path = Path("test/data/output/states_flow_output.net")
    output_path.parent.mkdir(exist_ok=True)
    im.write_pajek(str(output_path), flow=True)
    assert (filecmp.cmp(output_path, "test/data/states_flow_output.net"))


def test_recorded_teleportation_codelength_is_stable_across_trials():
    graph = nx.DiGraph()
    graph.add_edges_from(
        [(1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 6), (6, 4)]
    )

    def canonical_modules(modules):
        groups = defaultdict(list)
        for node_id, module_id in modules.items():
            groups[module_id].append(node_id)
        return sorted(sorted(group) for group in groups.values())

    def run(num_trials):
        im = Infomap(
            silent=True,
            recorded_teleportation=True,
            num_trials=num_trials,
        )
        im.add_networkx_graph(graph)
        im.run()
        return im

    one_trial = run(1)
    two_trials = run(2)

    assert canonical_modules(one_trial.get_modules()) == canonical_modules(
        two_trials.get_modules()
    )
    assert math.isclose(one_trial.codelength, two_trials.codelength)
    assert math.isclose(one_trial.index_codelength, two_trials.index_codelength)
