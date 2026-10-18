from collections import defaultdict

import pytest

pytestmark = pytest.mark.fast


def children_of_modules_of_modules(im):
    """The number of children of each module below the root that has sub-modules."""
    children = defaultdict(set)
    for node in im.get_nodes():
        modules = node.path[:-1]
        for depth in range(1, len(modules)):
            children[modules[:depth]].add(modules[depth])
    return [len(sub_modules) for sub_modules in children.values()]


# --preferred-number-of-modules counts the top modules of the tree. The sub-searches used to
# carry it too and charge |K_sub - K| on their own module count, so every module of modules
# came out with exactly K children, and the split was kept on a cost the tree does not
# have (#1292).
@pytest.mark.parametrize(
    ("fixture", "network", "preference"),
    [
        ("network_fixture_path", "unbalanced_hierarchy.net", 2),
        ("network_fixture_path", "unbalanced_hierarchy.net", 3),
        ("example_network_path", "ninetriangles.net", 2),
    ],
)
@pytest.mark.parametrize("seed", [1, 2, 3])
def test_sub_searches_do_not_charge_the_preference(
    request, make_infomap, tmp_path, fixture, network, preference, seed
):
    path = str(request.getfixturevalue(fixture)(network))
    im = make_infomap(seed=seed, num_trials=3, preferred_number_of_modules=preference)
    im.read_file(path)
    im.run()

    sub_module_counts = children_of_modules_of_modules(im)
    assert sub_module_counts
    assert set(sub_module_counts) != {preference}

    tree = tmp_path / "result.tree"
    im.write_tree(str(tree))
    back = make_infomap(
        no_infomap=True, cluster_data=str(tree), preferred_number_of_modules=preference
    )
    back.read_file(path)
    back.run()
    assert back.codelength == pytest.approx(im.codelength, abs=1e-10)
