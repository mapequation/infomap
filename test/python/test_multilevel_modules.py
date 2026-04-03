from collections import defaultdict

import pytest


pytestmark = pytest.mark.fast


def multilevel_modules(im, states):
    modules = defaultdict(list)

    for level in range(0, im.num_levels - 1):
        module_id = 1
        prev_path = None
        for node in im.get_nodes(states=states):
            if prev_path is None:
                prev_path = node.path
            index = min(level, len(node.path) - 2) + 1
            if node.path[:index] != prev_path[:index]:
                module_id += 1
            node_id = node.state_id if states else node.node_id
            modules[node_id].append(module_id)
            prev_path = node.path

    return {node: tuple(m) for node, m in modules.items()}


def test_multilevel_modules_states(make_infomap, fixture_path):
    im = make_infomap(num_trials=5)
    im.read_file(str(fixture_path("multilayer.net")))
    im.run()

    assert im.num_top_modules == 2
    assert im.num_levels == 2
    assert im.get_multilevel_modules(states=True) == multilevel_modules(im, states=True)

    with pytest.raises(RuntimeError):
        im.get_multilevel_modules(states=False)


def test_multilevel_modules(make_infomap, example_network_path):
    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("ninetriangles.net")))
    im.run()

    assert im.num_top_modules == 5
    assert im.num_levels == 3
    assert sorted(im.get_multilevel_modules(states=False).values()) == sorted(
        multilevel_modules(im, states=False).values()
    )
