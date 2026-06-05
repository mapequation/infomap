import pytest


pytestmark = pytest.mark.fast


def test_add_links_accepts_columnar_numpy_arrays(make_infomap):
    np = pytest.importorskip("numpy")

    sources = np.array([1, 2, 3], dtype=np.uint32)
    targets = np.array([2, 3, 1], dtype=np.uint32)
    weights = np.array([1.0, 2.0, 3.0], dtype=np.float64)

    im = make_infomap()
    im.add_links(sources, targets, weights)

    assert sorted(im.get_links()) == [(1, 2, 1.0), (2, 3, 2.0), (3, 1, 3.0)]


def test_add_links_accepts_columnar_python_lists(make_infomap):
    im = make_infomap()

    im.add_links([1, 2, 3], [2, 3, 1], [1.0, 2.0, 3.0])

    assert sorted(im.get_links()) == [(1, 2, 1.0), (2, 3, 2.0), (3, 1, 3.0)]


def test_add_links_rejects_mismatched_column_lengths(make_infomap):
    np = pytest.importorskip("numpy")

    im = make_infomap()

    with pytest.raises(ValueError, match="same length"):
        im.add_links(np.array([1, 2]), np.array([2]))


def test_add_link_table_accepts_pandas_dataframe(make_infomap):
    pd = pytest.importorskip("pandas")

    table = pd.DataFrame(
        {
            "source": [1, 2, 3],
            "target": [2, 3, 1],
            "weight": [1.0, 2.0, 3.0],
        }
    )

    im = make_infomap()
    im.add_link_table(table)

    assert sorted(im.get_links()) == [(1, 2, 1.0), (2, 3, 2.0), (3, 1, 3.0)]


def test_add_link_table_accepts_unweighted_table(make_infomap):
    pd = pytest.importorskip("pandas")

    table = pd.DataFrame({"source": [1, 2, 3], "target": [2, 3, 1]})

    im = make_infomap()
    im.add_link_table(table, weight_col=None)

    assert sorted(im.get_links()) == [(1, 2, 1.0), (2, 3, 1.0), (3, 1, 1.0)]


def test_add_link_table_requires_explicit_unweighted_mode(make_infomap):
    pd = pytest.importorskip("pandas")

    table = pd.DataFrame({"source": [1], "target": [2]})
    im = make_infomap()

    with pytest.raises(ValueError, match="Missing link table column 'weight'"):
        im.add_link_table(table)
