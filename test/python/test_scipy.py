from __future__ import annotations

import builtins
import math

import pytest
from infomap import Infomap
from infomap.io.scipy import _import_sparse

scipy = pytest.importorskip("scipy")
sp = scipy.sparse


def test_missing_scipy_error_mentions_extra(monkeypatch):
    def missing_scipy_import(name, *args, **kwargs):
        if name == "scipy.sparse":
            raise ImportError("missing scipy")
        return original_import(name, *args, **kwargs)

    original_import = builtins.__import__
    monkeypatch.setattr(builtins, "__import__", missing_scipy_import)

    with pytest.raises(ImportError, match=r"infomap\[scipy\]"):
        _import_sparse()


def test_add_scipy_sparse_matrix_accepts_csr_csc_and_coo(make_infomap):
    for matrix_factory in (sp.csr_matrix, sp.csc_matrix, sp.coo_matrix):
        im = make_infomap(no_infomap=True)
        mapping = im.add_scipy_sparse_matrix(matrix_factory([[0, 1], [1, 0]]))

        assert mapping == {0: 0, 1: 1}
        assert im.num_links == 1


def test_add_scipy_sparse_matrix_accepts_sparse_arrays_when_available(make_infomap):
    csr_array = getattr(sp, "csr_array", None)
    if csr_array is None:
        pytest.skip("SciPy sparse arrays are not available")

    im = make_infomap(no_infomap=True)
    mapping = im.add_scipy_sparse_matrix(csr_array([[0, 1], [1, 0]]))

    assert mapping == {0: 0, 1: 1}
    assert im.num_links == 1


def test_add_scipy_sparse_matrix_rejects_dense_arrays(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="scipy sparse"):
        im.add_scipy_sparse_matrix([[0, 1], [1, 0]])


def test_add_scipy_sparse_matrix_rejects_non_square(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="square"):
        im.add_scipy_sparse_matrix(sp.csr_matrix((2, 3)))


def test_add_scipy_sparse_matrix_rejects_invalid_node_ids_length(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="node_ids"):
        im.add_scipy_sparse_matrix(sp.csr_matrix((2, 2)), node_ids=["a"])


def test_add_scipy_sparse_matrix_rejects_duplicate_node_ids(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="unique"):
        im.add_scipy_sparse_matrix(sp.csr_matrix((2, 2)), node_ids=["a", "a"])


def test_add_scipy_sparse_matrix_rejects_nan_weights(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="NaN"):
        im.add_scipy_sparse_matrix(
            sp.coo_matrix(([math.nan], ([0], [1])), shape=(2, 2))
        )


def test_add_scipy_sparse_matrix_rejects_negative_weights(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="negative"):
        im.add_scipy_sparse_matrix(sp.coo_matrix(([-1.0], ([0], [1])), shape=(2, 2)))


def test_from_scipy_sparse_matrix_constructs_infomap_with_args():
    im = Infomap.from_scipy_sparse_matrix(
        sp.csr_matrix([[0, 1], [1, 0]]),
        args="--silent --no-infomap",
    )

    assert isinstance(im, Infomap)
    assert im.num_links == 1


def test_add_scipy_sparse_matrix_returns_custom_node_id_mapping(make_infomap):
    im = make_infomap(no_infomap=True)

    mapping = im.add_scipy_sparse_matrix(
        sp.coo_matrix([[0, 3], [3, 0]]),
        directed=False,
        node_ids=["gene_a", "gene_b"],
    )

    im.run()
    modules = {
        mapping[node_id]: module_id for node_id, module_id in im.get_modules().items()
    }

    assert mapping == {0: "gene_a", 1: "gene_b"}
    assert set(modules) == {"gene_a", "gene_b"}


def test_add_scipy_sparse_matrix_directed_adds_each_nonzero(make_infomap):
    im = make_infomap(no_infomap=True)

    im.add_scipy_sparse_matrix(sp.csr_matrix([[0, 2], [3, 0]]), directed=True)

    assert im.num_links == 2


def test_add_scipy_sparse_matrix_undirected_deduplicates_symmetric_entries(
    make_infomap,
):
    im = make_infomap(no_infomap=True)

    im.add_scipy_sparse_matrix(sp.csr_matrix([[0, 2], [2, 0]]), directed=False)

    assert im.num_links == 1


def test_add_scipy_sparse_matrix_undirected_accepts_upper_triangular(make_infomap):
    im = make_infomap(no_infomap=True)

    im.add_scipy_sparse_matrix(
        sp.csr_matrix([[0, 2, 0], [0, 0, 4], [0, 0, 0]]), directed=False
    )

    assert im.num_links == 2


def test_add_scipy_sparse_matrix_unweighted_ignores_values(make_infomap):
    weighted = make_infomap(no_infomap=True)
    unweighted = make_infomap(no_infomap=True)

    weighted.add_scipy_sparse_matrix(
        sp.csr_matrix([[0, 5], [0, 0]]), directed=True, weighted=True
    )
    unweighted.add_scipy_sparse_matrix(
        sp.csr_matrix([[0, 5], [0, 0]]), directed=True, weighted=False
    )

    assert weighted.num_links == 1
    assert unweighted.num_links == 1


def test_from_scipy_sparse_matrix_runs_empty_graph_with_nodes():
    im = Infomap.from_scipy_sparse_matrix(sp.csr_matrix((3, 3)), args="--silent")

    im.run()

    assert set(im.get_modules()) == {0, 1, 2}


def test_from_scipy_sparse_matrix_runs_single_node_graph():
    im = Infomap.from_scipy_sparse_matrix(sp.coo_matrix((1, 1)), args="--silent")

    im.run()

    assert im.get_modules() == {0: 1}


def test_from_scipy_sparse_matrix_weighted_graph_returns_all_nodes():
    matrix = sp.csr_matrix(
        [
            [0, 1, 0, 0],
            [1, 0, 4, 0],
            [0, 4, 0, 1],
            [0, 0, 1, 0],
        ]
    )
    im = Infomap.from_scipy_sparse_matrix(
        matrix, args="--silent --num-trials 2 --seed 123"
    )

    im.run()

    assert set(im.get_modules()) == {0, 1, 2, 3}
