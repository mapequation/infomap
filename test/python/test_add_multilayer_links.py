import pytest
from infomap import MultilayerNode


pytestmark = pytest.mark.fast


def assert_same_multilayer_links(actual, expected):
    assert actual.num_links == expected.num_links
    assert actual.num_nodes == expected.num_nodes
    assert actual.num_physical_nodes == expected.num_physical_nodes
    assert sorted(actual.get_links()) == sorted(expected.get_links())


def test_add_multilayer_links_matches_repeated_add_multilayer_link(make_infomap):
    links = [
        ((0, 1), (1, 2), 1.5),
        ((0, 3), (1, 2), 2.0),
        (MultilayerNode(layer_id=1, node_id=2), MultilayerNode(layer_id=2, node_id=4), 3.0),
    ]

    baseline = make_infomap()
    for link in links:
        baseline.add_multilayer_link(*link)

    im = make_infomap()
    im.add_multilayer_links(links)

    assert_same_multilayer_links(im, baseline)


def test_add_multilayer_links_uses_default_weight(make_infomap):
    links = [((0, 1), (1, 2)), ((0, 3), (1, 2))]

    baseline = make_infomap()
    for link in links:
        baseline.add_multilayer_link(*link)

    im = make_infomap()
    im.add_multilayer_links(links)

    assert_same_multilayer_links(im, baseline)


def test_add_multilayer_links_accepts_weighted_numpy_array(make_infomap):
    np = pytest.importorskip("numpy")
    links = np.array(
        [
            [0, 1, 1, 2, 1.5],
            [0, 3, 1, 2, 2.0],
            [1, 2, 2, 4, 3.0],
        ]
    )

    baseline = make_infomap()
    for source_layer, source_node, target_layer, target_node, weight in links.tolist():
        baseline.add_multilayer_link(
            (source_layer, source_node), (target_layer, target_node), weight
        )

    im = make_infomap()
    im.add_multilayer_links(links)

    assert_same_multilayer_links(im, baseline)


def test_add_multilayer_links_accepts_unweighted_numpy_array(make_infomap):
    np = pytest.importorskip("numpy")
    links = np.array([[0, 1, 1, 2], [0, 3, 1, 2]], dtype=np.uint32)

    baseline = make_infomap()
    for source_layer, source_node, target_layer, target_node in links.tolist():
        baseline.add_multilayer_link((source_layer, source_node), (target_layer, target_node))

    im = make_infomap()
    im.add_multilayer_links(links)

    assert_same_multilayer_links(im, baseline)


def test_add_multilayer_links_accepts_non_contiguous_numpy_array(make_infomap):
    np = pytest.importorskip("numpy")
    links = np.array(
        [
            [0, 1, 1, 2, 1.5, 99.0],
            [0, 3, 1, 2, 2.0, 99.0],
            [1, 2, 2, 4, 3.0, 99.0],
        ]
    )[:, :5]

    baseline = make_infomap()
    for source_layer, source_node, target_layer, target_node, weight in links.tolist():
        baseline.add_multilayer_link(
            (source_layer, source_node), (target_layer, target_node), weight
        )

    im = make_infomap()
    im.add_multilayer_links(links)

    assert_same_multilayer_links(im, baseline)


def test_add_multilayer_links_rejects_invalid_link_lengths(make_infomap):
    im = make_infomap()

    with pytest.raises(ValueError, match="2 or 3 values"):
        im.add_multilayer_links([((0, 1),)])

    with pytest.raises(ValueError, match="2 or 3 values"):
        im.add_multilayer_links([((0, 1), (1, 2), 1.0, 2.0)])


def test_add_multilayer_links_rejects_invalid_node_lengths(make_infomap):
    im = make_infomap()

    with pytest.raises(ValueError, match="multilayer node.*2 values"):
        im.add_multilayer_links([((0,), (1, 2))])

    with pytest.raises(ValueError, match="multilayer node.*2 values"):
        im.add_multilayer_links([((0, 1), (1, 2, 3))])


def test_add_multilayer_links_rejects_invalid_numpy_shapes(make_infomap):
    np = pytest.importorskip("numpy")
    im = make_infomap()

    with pytest.raises(ValueError, match="2D"):
        im.add_multilayer_links(np.array([0, 1, 1, 2]))

    with pytest.raises(ValueError, match="4 or 5 columns"):
        im.add_multilayer_links(np.array([[0, 1, 1]]))

    with pytest.raises(ValueError, match="numeric dtype"):
        im.add_multilayer_links(np.array([["0", "1", "1", "2"]]))
