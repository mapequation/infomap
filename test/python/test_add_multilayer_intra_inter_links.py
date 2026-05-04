import pytest


pytestmark = pytest.mark.fast


INTRA_LINKS = [
    (1, 1, 2, 1.0),
    (1, 2, 3, 1.0),
    (1, 3, 1, 1.0),
    (2, 1, 2, 2.0),
    (2, 2, 3, 2.0),
    (2, 3, 1, 2.0),
]

INTER_LINKS = [
    (1, 1, 2, 0.5),
    (1, 2, 2, 0.5),
    (1, 3, 2, 0.5),
]


def assert_same_result(actual, expected):
    actual.run()
    expected.run()

    assert actual.num_links == expected.num_links
    assert actual.num_nodes == expected.num_nodes
    assert actual.num_physical_nodes == expected.num_physical_nodes
    assert actual.codelength == pytest.approx(expected.codelength)
    assert actual.get_modules(states=True) == expected.get_modules(states=True)


def test_add_multilayer_intra_links_matches_repeated_add_multilayer_intra_link(make_infomap):
    baseline = make_infomap(two_level=True)
    for link in INTRA_LINKS:
        baseline.add_multilayer_intra_link(*link)

    im = make_infomap(two_level=True)
    im.add_multilayer_intra_links(INTRA_LINKS)

    assert_same_result(im, baseline)


def test_add_multilayer_intra_links_accepts_numpy_arrays(make_infomap):
    np = pytest.importorskip("numpy")

    baseline = make_infomap(two_level=True)
    for link in INTRA_LINKS:
        baseline.add_multilayer_intra_link(*link)

    im = make_infomap(two_level=True)
    im.add_multilayer_intra_links(np.array(INTRA_LINKS))

    assert_same_result(im, baseline)


def test_add_multilayer_intra_links_numpy_default_weight(make_infomap):
    np = pytest.importorskip("numpy")
    links = [(1, 1, 2), (1, 2, 3), (1, 3, 1)]

    baseline = make_infomap(two_level=True)
    for link in links:
        baseline.add_multilayer_intra_link(*link)

    im = make_infomap(two_level=True)
    im.add_multilayer_intra_links(np.array(links, dtype=np.uint32))

    assert_same_result(im, baseline)


def test_add_multilayer_inter_links_matches_repeated_add_multilayer_inter_link(make_infomap):
    baseline = make_infomap(two_level=True)
    baseline.add_multilayer_intra_links(INTRA_LINKS)
    for link in INTER_LINKS:
        baseline.add_multilayer_inter_link(*link)

    im = make_infomap(two_level=True)
    im.add_multilayer_intra_links(INTRA_LINKS)
    im.add_multilayer_inter_links(INTER_LINKS)

    assert_same_result(im, baseline)


def test_add_multilayer_inter_links_accepts_numpy_arrays(make_infomap):
    np = pytest.importorskip("numpy")

    baseline = make_infomap(two_level=True)
    baseline.add_multilayer_intra_links(INTRA_LINKS)
    for link in INTER_LINKS:
        baseline.add_multilayer_inter_link(*link)

    im = make_infomap(two_level=True)
    im.add_multilayer_intra_links(INTRA_LINKS)
    im.add_multilayer_inter_links(np.array(INTER_LINKS))

    assert_same_result(im, baseline)


def test_add_multilayer_inter_links_numpy_default_weight(make_infomap):
    np = pytest.importorskip("numpy")
    links = [(1, 1, 2), (1, 2, 2), (1, 3, 2)]

    baseline = make_infomap(two_level=True)
    baseline.add_multilayer_intra_links(INTRA_LINKS)
    for link in links:
        baseline.add_multilayer_inter_link(*link)

    im = make_infomap(two_level=True)
    im.add_multilayer_intra_links(INTRA_LINKS)
    im.add_multilayer_inter_links(np.array(links, dtype=np.uint32))

    assert_same_result(im, baseline)


def test_add_multilayer_intra_links_rejects_invalid_input(make_infomap):
    np = pytest.importorskip("numpy")
    im = make_infomap()

    with pytest.raises(ValueError, match="3 or 4 values"):
        im.add_multilayer_intra_links([(1, 2)])

    with pytest.raises(ValueError, match="3 or 4 values"):
        im.add_multilayer_intra_links([(1, 2, 3, 4, 5)])

    with pytest.raises(ValueError, match="2D"):
        im.add_multilayer_intra_links(np.array([1, 2, 3]))

    with pytest.raises(ValueError, match="3 or 4 columns"):
        im.add_multilayer_intra_links(np.array([[1, 2]]))


def test_add_multilayer_inter_links_rejects_invalid_input(make_infomap):
    np = pytest.importorskip("numpy")
    im = make_infomap()

    with pytest.raises(ValueError, match="3 or 4 values"):
        im.add_multilayer_inter_links([(1, 2)])

    with pytest.raises(ValueError, match="3 or 4 values"):
        im.add_multilayer_inter_links([(1, 2, 3, 4, 5)])

    with pytest.raises(ValueError, match="2D"):
        im.add_multilayer_inter_links(np.array([1, 2, 3]))

    with pytest.raises(ValueError, match="3 or 4 columns"):
        im.add_multilayer_inter_links(np.array([[1, 2]]))


def test_add_multilayer_inter_links_preserves_native_layer_validation(make_infomap):
    im = make_infomap()

    with pytest.raises(RuntimeError, match="must have layer1 != layer2"):
        im.add_multilayer_inter_links([(1, 1, 1)])
