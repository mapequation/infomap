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


def test_add_links_matches_repeated_add_link(make_infomap, canonical_modules):
    links = [
        (1, 2, 2.0),
        (1, 3, 2.0),
        (2, 3, 2.0),
        (3, 4, 1.0),
        (4, 5, 3.0),
        (4, 6, 3.0),
        (5, 6, 3.0),
    ]

    baseline = make_infomap()
    for link in links:
        baseline.add_link(*link)
    baseline.run()

    im = make_infomap()
    im.add_links(links)
    im.run()

    assert im.num_links == baseline.num_links
    assert im.num_nodes == baseline.num_nodes
    assert im.codelength == pytest.approx(baseline.codelength)
    assert canonical_modules(im.get_modules()) == canonical_modules(
        baseline.get_modules()
    )


def test_add_links_rejects_invalid_link_lengths(make_infomap):
    im = make_infomap()

    with pytest.raises(ValueError, match="2 or 3 values"):
        im.add_links([(1,)])

    with pytest.raises(ValueError, match="2 or 3 values"):
        im.add_links([(1, 2, 3, 4)])


def test_add_links_keeps_single_argument_api(make_infomap):
    im = make_infomap()

    with pytest.raises(TypeError):
        im.add_links([1], [2], [1.0])


def test_add_links_accepts_weighted_numpy_array(make_infomap, canonical_modules):
    np = pytest.importorskip("numpy")

    links = np.array(
        [
            [1, 2, 2.0],
            [1, 3, 2.0],
            [2, 3, 2.0],
            [3, 4, 1.0],
            [4, 5, 3.0],
            [4, 6, 3.0],
            [5, 6, 3.0],
        ]
    )

    baseline = make_infomap()
    for link in links.tolist():
        baseline.add_link(*link)
    baseline.run()

    im = make_infomap()
    im.add_links(links)
    im.run()

    assert im.num_links == baseline.num_links
    assert im.num_nodes == baseline.num_nodes
    assert im.codelength == pytest.approx(baseline.codelength)
    assert canonical_modules(im.get_modules()) == canonical_modules(
        baseline.get_modules()
    )


def test_add_links_accepts_numpy_array_with_duplicates_and_existing_links(make_infomap):
    np = pytest.importorskip("numpy")

    links = np.array(
        [
            [3, 4, 2.0],
            [1, 2, 1.0],
            [1, 2, 2.0],
            [2, 2, 5.0],
            [4, 4, 0.4],
            [7, 8, 0.0],
            [9, 9, -1.0],
            [1, 2, 3.0],
        ]
    )

    baseline = make_infomap(weight_threshold=0.5)
    baseline.add_link(1, 2, 10.0)
    baseline.add_link(10, 11, 1.0)
    for link in links.tolist():
        baseline.add_link(*link)

    im = make_infomap(weight_threshold=0.5)
    im.add_link(1, 2, 10.0)
    im.add_link(10, 11, 1.0)
    im.add_links(links)

    assert im.num_links == baseline.num_links
    assert im.num_nodes == baseline.num_nodes
    assert sorted(im.get_links()) == sorted(baseline.get_links())


def test_add_links_accepts_unweighted_numpy_array(make_infomap):
    np = pytest.importorskip("numpy")

    im = make_infomap()
    im.add_links(np.array([[1, 2], [2, 3], [3, 1]], dtype=np.uint32))
    im.run()

    assert im.num_links == 3
    assert im.num_nodes == 3


def test_add_links_accepts_non_contiguous_numpy_array(make_infomap):
    np = pytest.importorskip("numpy")

    links = np.array(
        [
            [0, 1, 1.0, 99.0],
            [1, 2, 1.0, 99.0],
            [2, 0, 1.0, 99.0],
        ]
    )[:, :3]

    im = make_infomap()
    im.add_links(links)
    im.run()

    assert im.num_links == 3
    assert im.num_nodes == 3


def test_add_links_rejects_invalid_numpy_shapes(make_infomap):
    np = pytest.importorskip("numpy")
    im = make_infomap()

    with pytest.raises(ValueError, match="2D"):
        im.add_links(np.array([1, 2, 3]))

    with pytest.raises(ValueError, match="2 or 3 columns"):
        im.add_links(np.array([[1, 2, 3, 4]]))

    with pytest.raises(ValueError, match="numeric dtype"):
        im.add_links(np.array([["1", "2"]]))

    with pytest.raises(ValueError, match="32-bit or 64-bit"):
        im.add_links(np.array([[1, 2]], dtype=np.uint16))
