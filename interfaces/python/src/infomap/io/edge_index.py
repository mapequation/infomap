from __future__ import annotations

from collections.abc import Iterable, Sequence
from numbers import Integral
from typing import Any

from .._optional import require_numpy
from ._arrays import undirected_edge_items


def _as_numpy_array(value: Any, *, name: str):
    np = require_numpy("edge_index support")

    if hasattr(value, "detach") and callable(value.detach):
        # Duck-typed torch.Tensor: detach().cpu().numpy(). The hasattr narrowing
        # would otherwise collapse the result of detach() to ``object``; keep it
        # ``Any`` so the chained tensor methods type-check soundly.
        tensor: Any = value
        try:
            value = tensor.detach().cpu().numpy()
        except AttributeError as exc:
            raise ValueError(
                f"`{name}` tensor inputs must support detach().cpu().numpy()."
            ) from exc

    try:
        return np.asarray(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"`{name}` must be array-like.") from exc


def _validate_edge_index(edge_index: Any):
    edge_index_array = _as_numpy_array(edge_index, name="edge_index")
    if edge_index_array.ndim != 2 or edge_index_array.shape[0] != 2:
        raise ValueError("`edge_index` must have shape `(2, num_edges)`.")
    if edge_index_array.dtype.kind not in "iu":
        raise ValueError("`edge_index` values must be integer node ids.")
    if bool((edge_index_array < 0).any()):
        raise ValueError("`edge_index` cannot contain negative node ids.")
    return edge_index_array


def _validate_num_nodes(edge_index: Any, num_nodes: int | None) -> int:
    if num_nodes is not None and (
        not isinstance(num_nodes, Integral) or isinstance(num_nodes, bool)
    ):
        raise ValueError("`num_nodes` must be an integer.")

    if edge_index.shape[1] == 0:
        if num_nodes is None:
            return 0
        if num_nodes < 0:
            raise ValueError("`num_nodes` cannot be negative.")
        return int(num_nodes)

    inferred_num_nodes = int(edge_index.max()) + 1
    if num_nodes is None:
        return inferred_num_nodes
    if num_nodes < inferred_num_nodes:
        raise ValueError(
            "`num_nodes` cannot be smaller than the largest node id plus one."
        )
    return int(num_nodes)


def _validate_node_ids(node_ids: Sequence[Any] | None, n_nodes: int) -> list[Any]:
    if node_ids is None:
        return list(range(n_nodes))

    labels = list(node_ids)
    if len(labels) != n_nodes:
        raise ValueError("`node_ids` length must match `num_nodes`.")
    if len(set(labels)) != len(labels):
        raise ValueError("`node_ids` values must be unique.")
    return labels


def _validate_edge_weight(edge_weight: Any, num_edges: int):
    import numpy as np

    if edge_weight is None:
        return None

    weights = _as_numpy_array(edge_weight, name="edge_weight")
    if weights.ndim != 1 or weights.shape[0] != num_edges:
        raise ValueError(
            "`edge_weight` must be a one-dimensional array with one value per edge."
        )
    if weights.dtype.kind not in "iuf":
        raise ValueError("`edge_weight` values must be numeric.")
    if bool(np.isnan(weights).any()):
        raise ValueError("`edge_weight` contains NaN weights.")
    if bool((weights < 0).any()):
        raise ValueError(
            "`edge_weight` contains negative weights, which Infomap does not support."
        )
    return weights


def _edge_items(edge_index: Any, weights: Any, *, directed: bool):
    # Normalise both weighted/unweighted shapes to fixed (source, target, weight)
    # integer-coerced triples so the directed and undirected paths share one form.
    triples: Iterable[tuple[int, int, float]]
    if weights is None:
        triples = (
            (int(source), int(target), 1.0)
            for source, target in zip(edge_index[0], edge_index[1], strict=True)
        )
    else:
        triples = (
            (int(source), int(target), float(weight))
            for source, target, weight in zip(
                edge_index[0], edge_index[1], weights, strict=True
            )
        )

    if directed:
        yield from triples
        return
    yield from undirected_edge_items(triples)


def add_edge_index(
    infomap: Any,
    edge_index: Any,
    *,
    edge_weight: Any = None,
    num_nodes: int | None = None,
    directed: bool = True,
    node_ids: Sequence[Any] | None = None,
) -> dict[int, Any]:
    """Add links and nodes from a PyG-style ``edge_index``.

    Directedness
    ------------
    Controlled by the ``directed`` parameter, which defaults to ``True``
    (each column of ``edge_index`` is a directed source->target edge). This
    default differs from the other adapters: networkx and igraph auto-detect via
    ``is_directed()``, and ``add_scipy_sparse_matrix`` defaults ``directed=False``.

    Weight parameter
    ----------------
    This adapter names its edge-weight parameter ``edge_weight`` (a
    one-dimensional array with one value per edge, or ``None`` for unit
    weights). The other adapters use different names: networkx ``weight``,
    igraph ``edge_weights``, scipy ``weighted`` (a bool).
    """
    edge_index_array = _validate_edge_index(edge_index)
    n_nodes = _validate_num_nodes(edge_index_array, num_nodes)
    labels = _validate_node_ids(node_ids, n_nodes)
    weights = _validate_edge_weight(edge_weight, edge_index_array.shape[1])

    internal_to_label = {index: label for index, label in enumerate(labels)}

    if not infomap._core.flowModelIsSet and directed:
        infomap._core.setDirected(True)

    for internal_id, label in internal_to_label.items():
        infomap.add_node(internal_id, name=label if isinstance(label, str) else None)

    for source, target, weight in _edge_items(
        edge_index_array, weights, directed=directed
    ):
        infomap.add_link(source, target, weight)

    return internal_to_label
