import pytest


pytestmark = pytest.mark.fast


def test_add_link_smoke(make_infomap, load_graph_fixture, canonical_modules):
    im = make_infomap()
    load_graph_fixture(im, "twotriangles_unweighted.edges", method="add_link")

    im.run()

    assert im.num_top_modules == 2
    assert canonical_modules(im.get_modules()) == [[1, 2, 3], [4, 5, 6]]


def test_add_links_smoke(make_infomap, load_graph_fixture, canonical_modules):
    im = make_infomap()
    load_graph_fixture(im, "twotriangles_weighted.edges", method="add_links")

    im.run()

    assert im.num_top_modules == 2
    assert canonical_modules(im.get_modules()) == [[1, 2, 3], [4, 5, 6]]
