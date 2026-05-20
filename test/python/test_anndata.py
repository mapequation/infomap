from __future__ import annotations

from types import SimpleNamespace

import pytest

import infomap

ad = pytest.importorskip("anndata")
pd = pytest.importorskip("pandas")
scipy = pytest.importorskip("scipy")
sp = scipy.sparse

pytestmark = pytest.mark.fast


def _adata(*, key: str = "connectivities", matrix=None):
    if matrix is None:
        matrix = sp.csr_matrix(
            [
                [0, 1, 0, 0],
                [1, 0, 0, 0],
                [0, 0, 0, 1],
                [0, 0, 1, 0],
            ]
        )
    data = ad.AnnData(obs=pd.DataFrame(index=["cell-a", "cell-b", "cell-c", "cell-d"]))
    data.obsp[key] = matrix
    data.uns["neighbors"] = {"connectivities_key": key}
    return data


def test_tl_infomap_mutates_anndata_in_place():
    data = _adata()

    result = infomap.tl.infomap(data, seed=123, num_trials=1)

    assert result is None
    assert "infomap" in data.obs
    assert str(data.obs["infomap"].dtype) == "category"
    assert set(data.obs["infomap"].astype(str)) == {"1", "2"}


def test_tl_infomap_copy_returns_modified_copy_only():
    data = _adata()

    copied = infomap.tl.infomap(data, copy=True, seed=123, num_trials=1)

    assert copied is not data
    assert "infomap" not in data.obs
    assert "infomap" in copied.obs


def test_tl_infomap_uses_custom_key_added():
    data = _adata()

    infomap.tl.infomap(data, key_added="infomap_clusters", seed=123, num_trials=1)

    assert "infomap_clusters" in data.obs
    assert "infomap_clusters" in data.uns
    assert "infomap" not in data.obs


def test_tl_infomap_uses_custom_obsp_key():
    data = _adata(key="custom_connectivities")

    infomap.tl.infomap(data, obsp="custom_connectivities", seed=123, num_trials=1)

    assert data.uns["infomap"]["params"]["obsp"] == "custom_connectivities"


def test_tl_infomap_resolves_neighbors_key():
    data = _adata(key="custom_connectivities")

    infomap.tl.infomap(data, neighbors_key="neighbors", seed=123, num_trials=1)

    assert data.uns["infomap"]["params"]["neighbors_key"] == "neighbors"
    assert data.uns["infomap"]["params"]["obsp"] == "custom_connectivities"


def test_tl_infomap_accepts_explicit_adjacency_matrix():
    data = _adata()

    infomap.tl.infomap(
        data,
        adjacency=data.obsp["connectivities"],
        key_added="from_matrix",
        seed=123,
        num_trials=1,
    )

    assert "from_matrix" in data.obs
    assert data.uns["from_matrix"]["params"]["obsp"] is None


def test_tl_infomap_rejects_ambiguous_graph_inputs():
    data = _adata()

    with pytest.raises(ValueError, match="only one"):
        infomap.tl.infomap(
            data, adjacency=data.obsp["connectivities"], obsp="connectivities"
        )

    with pytest.raises(ValueError, match="only one"):
        infomap.tl.infomap(data, neighbors_key="neighbors", obsp="connectivities")


def test_tl_infomap_rejects_missing_obsp_key():
    data = _adata()

    with pytest.raises(ValueError, match=r"adata\.obsp\['missing'\]"):
        infomap.tl.infomap(data, obsp="missing")


def test_tl_infomap_rejects_missing_neighbors_key():
    data = _adata()

    with pytest.raises(ValueError, match=r"adata\.uns\['missing'\]"):
        infomap.tl.infomap(data, neighbors_key="missing")


def test_tl_infomap_rejects_neighbors_without_connectivities_key():
    data = _adata()
    data.uns["neighbors"] = {}

    with pytest.raises(ValueError, match="connectivities_key"):
        infomap.tl.infomap(data, neighbors_key="neighbors")


def test_tl_infomap_rejects_malformed_anndata_like_object():
    data = SimpleNamespace(obs={}, obsp={}, obs_names=[])

    with pytest.raises(ValueError, match=r"missing `\.uns`"):
        infomap.tl.infomap(data)


def test_tl_infomap_rejects_copy_without_copy_method():
    data = SimpleNamespace(
        obs=pd.DataFrame(index=["cell-a"]),
        obsp={"connectivities": sp.csr_matrix((1, 1))},
        uns={},
        obs_names=["cell-a"],
        n_obs=1,
    )

    with pytest.raises(ValueError, match=r"\.copy\(\)"):
        infomap.tl.infomap(data, copy=True)


def test_tl_infomap_rejects_non_square_adjacency():
    data = _adata(matrix=sp.csr_matrix((4, 4)))

    with pytest.raises(ValueError, match="square"):
        infomap.tl.infomap(data, adjacency=sp.csr_matrix((2, 3)))


def test_tl_infomap_rejects_shape_mismatch():
    data = _adata()

    with pytest.raises(ValueError, match="adata.n_obs"):
        infomap.tl.infomap(data, adjacency=sp.csr_matrix((3, 3)))


def test_tl_infomap_writes_metadata():
    data = _adata()

    infomap.tl.infomap(
        data,
        directed=True,
        use_weights=False,
        args="--silent",
        seed=123,
        num_trials=1,
    )

    metadata = data.uns["infomap"]
    assert metadata["params"] == {
        "directed": True,
        "use_weights": False,
        "neighbors_key": None,
        "obsp": "connectivities",
        "args": "--silent",
        "silent": True,
        "no_file_output": True,
        "seed": 123,
        "num_trials": 1,
    }
    assert metadata["n_modules"] == 2
    assert isinstance(metadata["codelength"], float)


def test_tl_infomap_preserves_obs_name_order_for_string_node_ids(monkeypatch):
    data = _adata()
    seen_node_ids = []

    original_add_scipy_sparse_matrix = infomap.Infomap.add_scipy_sparse_matrix

    def recording_add_scipy_sparse_matrix(self, matrix, **kwargs):
        seen_node_ids.extend(kwargs["node_ids"])
        return original_add_scipy_sparse_matrix(self, matrix, **kwargs)

    monkeypatch.setattr(
        infomap.Infomap,
        "add_scipy_sparse_matrix",
        recording_add_scipy_sparse_matrix,
    )

    infomap.tl.infomap(data, seed=123, num_trials=1)

    assert seen_node_ids == ["cell-a", "cell-b", "cell-c", "cell-d"]
    assert list(data.obs.index) == ["cell-a", "cell-b", "cell-c", "cell-d"]
