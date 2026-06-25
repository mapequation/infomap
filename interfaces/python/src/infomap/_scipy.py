from __future__ import annotations

from collections.abc import Sequence
from typing import Any


def _import_sparse() -> Any:
    try:
        import scipy.sparse as sparse
    except ImportError as exc:
        raise ImportError(
            "The 'scipy' package is required for SciPy sparse matrix support. "
            'Install it with `python -m pip install "infomap[scipy]"`.'
        ) from exc
    return sparse


def _validate_sparse_matrix(sparse: Any, A: Any) -> Any:
    if not sparse.issparse(A):
        raise ValueError(
            "`A` must be a scipy sparse matrix or sparse array, not a dense array."
        )
    if A.shape[0] != A.shape[1]:
        raise ValueError("`A` must be a square adjacency matrix.")
    return A


def _validate_node_ids(node_ids: Sequence[Any] | None, n_nodes: int) -> list[Any]:
    if node_ids is None:
        return list(range(n_nodes))

    labels = list(node_ids)
    if len(labels) != n_nodes:
        raise ValueError("`node_ids` length must match `A.shape[0]`.")
    if len(set(labels)) != len(labels):
        raise ValueError("`node_ids` values must be unique.")
    return labels


def _validate_weights(coo: Any) -> None:
    import numpy as np

    data = coo.data
    if data.size == 0:
        return
    if bool(np.isnan(data).any()):
        raise ValueError("`A` contains NaN weights.")
    if bool((data < 0).any()):
        raise ValueError(
            "`A` contains negative weights, which Infomap does not support."
        )


def _edge_items(coo: Any, *, directed: bool, weighted: bool):
    if directed:
        for source, target, value in zip(coo.row, coo.col, coo.data, strict=True):
            yield int(source), int(target), float(value) if weighted else 1.0
        return

    edges: dict[tuple[int, int], float] = {}
    for source, target, value in zip(coo.row, coo.col, coo.data, strict=True):
        source_id = int(source)
        target_id = int(target)
        edge = (
            (source_id, target_id) if source_id <= target_id else (target_id, source_id)
        )
        weight = float(value) if weighted else 1.0
        if edge in edges:
            edges[edge] = max(edges[edge], weight)
        else:
            edges[edge] = weight

    for (source, target), weight in edges.items():
        yield source, target, weight


def add_scipy_sparse_matrix(
    infomap: Any,
    A: Any,
    *,
    directed: bool = False,
    weighted: bool = True,
    node_ids: Sequence[Any] | None = None,
) -> dict[int, Any]:
    sparse = _import_sparse()
    matrix = _validate_sparse_matrix(sparse, A)
    labels = _validate_node_ids(node_ids, matrix.shape[0])
    coo = matrix.tocoo(copy=False)
    coo.sum_duplicates()
    _validate_weights(coo)

    internal_to_label = {index: label for index, label in enumerate(labels)}

    if not infomap._core.flowModelIsSet and directed:
        infomap._core.setDirected(True)

    for internal_id, label in internal_to_label.items():
        infomap.add_node(internal_id, name=label if isinstance(label, str) else None)

    for source, target, weight in _edge_items(
        coo, directed=directed, weighted=weighted
    ):
        infomap.add_link(source, target, weight)

    return internal_to_label
