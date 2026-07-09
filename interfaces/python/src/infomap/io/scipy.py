from __future__ import annotations

from collections.abc import Sequence
from typing import Any

from .._optional import require_scipy_sparse
from ._arrays import undirected_edge_items

# No user-facing API: the adapter (add_scipy_sparse_matrix) is reached through
# Network.from_scipy_sparse_matrix / infomap.run(A). Empty __all__ so the
# submodule stops surfacing its plumbing.
__all__: list[str] = []


def _import_sparse() -> Any:
    # Thin delegate kept for backwards compatibility (tests import it); the
    # shared guard lives in infomap._optional.
    return require_scipy_sparse()


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
    triples = (
        (int(source), int(target), float(value) if weighted else 1.0)
        for source, target, value in zip(coo.row, coo.col, coo.data, strict=True)
    )
    if directed:
        yield from triples
        return
    # The coo matrix already had sum_duplicates() applied, so same-(i,j) weights
    # are summed; undirected_edge_items then dedups across (i,j)/(j,i) keeping max.
    yield from undirected_edge_items(triples)


def add_scipy_sparse_matrix(
    infomap: Any,
    A: Any,
    *,
    directed: bool = False,
    weighted: bool = True,
    node_ids: Sequence[Any] | None = None,
) -> dict[int, Any]:
    """Add links and nodes from a SciPy sparse adjacency matrix.

    Directedness
    ------------
    Controlled by the ``directed`` parameter, which defaults to ``False``
    (undirected; ``A[i, j]`` and ``A[j, i]`` are folded together). This default
    differs from the other adapters: networkx and igraph auto-detect via
    ``is_directed()``, and ``add_edge_index`` defaults ``directed=True``.

    Weight parameter
    ----------------
    This adapter names its weight control ``weighted`` -- a bool: when ``True``
    (the default) the matrix values are used as link weights, when ``False``
    every nonzero entry is weight ``1.0``. The other adapters take a named
    column/attribute/array instead: networkx ``weight``, igraph ``edge_weights``,
    edge_index ``edge_weight``.
    """
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
