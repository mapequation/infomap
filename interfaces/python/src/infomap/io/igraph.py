from __future__ import annotations

import math
from collections.abc import Iterable
from numbers import Integral, Real
from typing import Any

from .._optional import require_igraph
from ._arrays import apply_node_meta_data, community_node_data

# The only user-facing name here; the adapter (add_igraph_graph) and helpers are
# reached through Network.from_igraph / infomap.run(). Curated so the submodule
# stops surfacing its plumbing (require_igraph, community_node_data, ...).
__all__ = ["find_igraph_communities"]


def _import_igraph() -> Any:
    # Thin delegate kept for backwards compatibility (tests import it); the
    # shared guard lives in infomap._optional.
    return require_igraph("igraph graph support")


def _validate_igraph_graph(g: Any) -> Any:
    ig = _import_igraph()
    if not isinstance(g, ig.Graph):
        raise TypeError("`g` must be an igraph.Graph.")
    return ig


def _is_missing(value: Any) -> bool:
    return value is None or (isinstance(value, Real) and math.isnan(value))


def _validate_weight_values(values: Iterable[Any], *, name: str) -> list[float]:
    weights = list(values)
    for value in weights:
        if _is_missing(value):
            raise ValueError(f"`{name}` cannot contain missing or NaN values.")
        if not isinstance(value, Real) or isinstance(value, bool):
            raise ValueError(f"`{name}` must contain numeric values.")
    return [float(value) for value in weights]


def _edge_weights(g: Any, edge_weights: str | Iterable[Any] | None) -> list[float]:
    if edge_weights is None:
        return [1.0] * g.ecount()

    if isinstance(edge_weights, str):
        if edge_weights not in g.es.attributes():
            raise ValueError(f"`edge_weights` edge attribute {edge_weights!r} does not exist.")
        return _validate_weight_values(g.es[edge_weights], name="edge_weights")

    try:
        weights = list(edge_weights)
    except TypeError as exc:
        raise ValueError(
            "`edge_weights` must be None, an igraph edge attribute name, "
            "or a sequence with one value per edge."
        ) from exc

    if len(weights) != g.ecount():
        raise ValueError("`edge_weights` must contain one value per edge.")
    return _validate_weight_values(weights, name="edge_weights")


def _vertex_attribute(g, attribute):
    if attribute not in g.vs.attributes():
        return None
    values = list(g.vs[attribute])
    if any(_is_missing(value) for value in values):
        raise ValueError(f"`{attribute}` vertex attribute cannot contain missing or NaN values.")
    return values


def _integer_like(value, *, name):
    if isinstance(value, bool):
        raise ValueError(f"`{name}` values must be integer-like or non-missing labels.")
    if isinstance(value, Integral):
        return int(value)
    # Non-Integral Real (e.g. a float): convert through float, which is what a
    # finite integer-valued Real already is here. ``Real`` is an ABC pyright
    # cannot convert directly, but ``float(value)`` is sound and equivalent.
    if isinstance(value, Real) and math.isfinite(value) and int(float(value)) == value:
        return int(float(value))
    raise ValueError(f"`{name}` values must be integer-like.")


def _node_ids(values):
    if all(isinstance(value, Real) and not isinstance(value, bool) for value in values):
        ids = [_integer_like(value, name="node_id") for value in values]
        # Coercing integer-valued floats to ints can make two textually distinct
        # labels (e.g. 1 and 1.0, or 2 and 2.0) collapse onto the same physical
        # id. Detect that here and name the colliding labels instead of silently
        # merging two vertices into one physical node.
        labels_by_id: dict[int, Any] = {}
        for label, _node_id in zip(values, ids):
            seen = labels_by_id.setdefault(_node_id, label)
            if repr(seen) != repr(label):
                raise ValueError(
                    f"`node_id` labels {seen!r} and {label!r} both map to physical "
                    f"id {_node_id}. Use distinct integer-valued node ids."
                )
        return ids, {_node_id: str(_node_id) for _node_id in dict.fromkeys(ids)}

    # Non-numeric (or mixed) labels: key by identity-as-written. Dict equality
    # would otherwise merge textually distinct but ``==``-equal keys (e.g. 1 and
    # 1.0, or 1 and True) into one physical node. Apply the same guard as the
    # all-integer branch above: collapse genuine duplicates, but reject a
    # repr-distinct collision instead of silently merging two vertices.
    label_to_id: dict[Any, int] = {}
    canonical: dict[Any, Any] = {}
    ids = []
    for label in values:
        if label in label_to_id:
            seen = canonical[label]
            if repr(seen) != repr(label):
                raise ValueError(
                    f"`node_id` labels {seen!r} and {label!r} are distinct but "
                    f"collide as the same key. Use distinct node ids."
                )
        else:
            label_to_id[label] = len(label_to_id)
            canonical[label] = label
        ids.append(label_to_id[label])
    names = {_node_id: str(label) for label, _node_id in label_to_id.items()}
    return ids, names


def _layer_ids(values):
    return [_integer_like(value, name="layer_id") for value in values]


def _vertex_names(g):
    if "name" not in g.vs.attributes():
        return None
    return [None if name is None else str(name) for name in g.vs["name"]]


def add_igraph_graph(
    infomap: Any,
    g: Any,
    *,
    edge_weights: str | Iterable[Any] | None = None,
    vertex_weights: Any = None,
    node_id: str = "node_id",
    layer_id: str = "layer_id",
    multilayer_inter_intra_format: bool = True,
    meta_attribute: str | None = None,
) -> dict[int, Any]:
    """Add a python-igraph graph to an Infomap instance.

    Directedness
    ------------
    Auto-detected from the graph: when ``g.is_directed()`` is true and no flow
    model has been set on the instance, the ``directed`` flag is enabled. (The
    graph-library adapters diverge here: networkx and igraph auto-detect via
    ``is_directed()``; ``add_scipy_sparse_matrix`` defaults ``directed=False``;
    ``add_edge_index`` defaults ``directed=True``.)

    Weight parameter
    ----------------
    This adapter names its edge-weight parameter ``edge_weights`` (an edge
    attribute name, an explicit per-edge sequence, or ``None`` for unit
    weights). The other adapters use different names: networkx ``weight``, scipy
    ``weighted`` (a bool), edge_index ``edge_weight``.
    """
    _validate_igraph_graph(g)
    if vertex_weights is not None:
        raise ValueError("`vertex_weights` is not supported by infomap's igraph adapter yet.")

    if not infomap._core.flowModelIsSet and g.is_directed():
        infomap._core.setDirected(True)

    names = _vertex_names(g)
    weights = _edge_weights(g, edge_weights)
    vertices = list(range(g.vcount()))
    phys_values = _vertex_attribute(g, node_id)
    layer_values = _vertex_attribute(g, layer_id)
    is_state_network = phys_values is not None
    is_multilayer_network = phys_values is not None and layer_values is not None

    phys_names = {}
    if is_state_network:
        phys, phys_names = _node_ids(phys_values)
    else:
        phys = vertices
    layers = _layer_ids(layer_values) if is_multilayer_network else None

    for _node_id, name in phys_names.items():
        infomap.set_name(_node_id, name)

    if is_multilayer_network:
        assert layers is not None  # implied by is_multilayer_network (see above)
        for vertex_id in vertices:
            infomap.network.add_multilayer_node(
                vertex_id,
                layers[vertex_id],
                phys[vertex_id],
                1.0,
            )
    elif is_state_network:
        for vertex_id in vertices:
            infomap.add_state_node(vertex_id, phys[vertex_id])
    else:
        for vertex_id in vertices:
            name = names[vertex_id] if names is not None else None
            infomap.add_node(vertex_id, name=name)

    edges = g.get_edgelist()
    if is_multilayer_network:
        assert layers is not None  # implied by is_multilayer_network (see above)
        for edge_index, (source, target) in enumerate(edges):
            source_layer_id = layers[source]
            target_layer_id = layers[target]
            source_node_id = phys[source]
            target_node_id = phys[target]
            weight = weights[edge_index]

            if multilayer_inter_intra_format:
                if source_layer_id == target_layer_id:
                    infomap.add_multilayer_intra_link(
                        source_layer_id,
                        source_node_id,
                        target_node_id,
                        weight,
                    )
                else:
                    if source_node_id != target_node_id:
                        raise ValueError(
                            "Multilayer intra/inter format does not support 'diagonal' links. "
                            "Use `multilayer_inter_intra_format=False`"
                        )
                    infomap.add_multilayer_inter_link(
                        source_layer_id,
                        source_node_id,
                        target_layer_id,
                        weight,
                    )
            else:
                infomap.network.add_multilayer_state_link(
                    source,
                    source_layer_id,
                    source_node_id,
                    target,
                    target_layer_id,
                    target_node_id,
                    weight,
                )
    else:
        for edge_index, (source, target) in enumerate(edges):
            infomap.add_link(source, target, weights[edge_index])

    if meta_attribute is not None:
        meta_values = _vertex_attribute(g, meta_attribute)
        if meta_values is None:
            raise ValueError(
                f"`meta_attribute` vertex attribute {meta_attribute!r} does not exist."
            )
        # add_igraph_graph registers one node per vertex using the vertex index
        # as the (state) id, so meta is keyed by vertex index.
        apply_node_meta_data(infomap, zip(vertices, meta_values))

    if names is None:
        return {vertex_id: vertex_id for vertex_id in vertices}
    return {vertex_id: names[vertex_id] for vertex_id in vertices}


def _membership_and_flows(infomap, g, node_mapping):
    # Map results back through the ``node_mapping`` the adapter built rather than
    # assuming the engine hands back exactly the dense ``state_id`` range
    # ``[0, g.vcount())``. ``add_igraph_graph`` registers one state node per
    # igraph vertex, using the vertex index as the state id, so the mapping's
    # keys are precisely the state ids that correspond to a real vertex.
    #
    # Multilayer runs can mint *additional* leaf state nodes (inter-layer
    # relaxation, matchable ids) whose state ids fall outside that range. The old
    # path skipped them with a numeric ``state_id >= vcount`` bounds check and
    # then raised "Could not align ..." if that left any real vertex unset.
    # Selecting the registered vertices via ``node_mapping`` membership is robust
    # to those extra ids and mirrors how the networkx helper maps results back.
    membership = [None] * g.vcount()
    flows = [None] * g.vcount()
    module_ids = {}

    for node in community_node_data(infomap):
        if node.state_id not in node_mapping:
            continue
        vertex_id = node.state_id
        if node.module_id not in module_ids:
            module_ids[node.module_id] = len(module_ids)
        membership[vertex_id] = module_ids[node.module_id]
        flows[vertex_id] = node.flow

    if any(value is None for value in membership):
        raise RuntimeError("Could not align Infomap membership with igraph vertices.")

    return membership, flows


def find_igraph_communities(
    g: Any,
    *,
    edge_weights: str | Iterable[Any] | None = None,
    vertex_weights: Any = None,
    trials: int | None = None,
    node_id: str = "node_id",
    layer_id: str = "layer_id",
    multilayer_inter_intra_format: bool = True,
    module_attribute: str | None = None,
    flow_attribute: str | None = None,
    meta_attribute: str | None = None,
    **infomap_options: Any,
) -> Any:
    """Find communities in a python-igraph graph.

    This helper builds an :class:`~infomap.Infomap` instance from ``g`` (via
    :meth:`Infomap.add_igraph_graph`), runs Infomap, and returns the top-level
    partition as an igraph clustering.

    Parameters
    ----------
    g : igraph.Graph
        A python-igraph graph.
    edge_weights : str, sequence, or None, optional
        Edge weight attribute name, explicit sequence with one value per
        edge, or ``None`` to treat every edge as weight 1. Default ``None``.
    vertex_weights : None, optional
        Accepted for igraph API familiarity but not supported yet.
    trials : int, optional
        Number of independent trials; the best solution is kept. Convenience
        alias for the ``num_trials`` Infomap option. Pass ``trials`` or
        ``num_trials``, not both; if neither is given the default is ``10``.
    node_id : str, optional
        Vertex attribute for physical node ids, implying a state network.
    layer_id : str, optional
        Vertex attribute for layer ids, implying a multilayer network when
        ``node_id`` is also present.
    multilayer_inter_intra_format : bool, optional
        Use intra/inter format to simulate inter-layer links. Default
        ``True``.
    module_attribute : str, optional
        If set, write each vertex's module id back to this vertex attribute
        on ``g``.
    flow_attribute : str, optional
        If set, write each vertex's flow back to this vertex attribute on
        ``g``.
    meta_attribute : str, optional
        Vertex attribute to read categorical metadata from. Values are
        encoded to integers in first-seen order and set as Infomap
        metadata; vertices with missing values are skipped. Raises
        :class:`ValueError` if the attribute does not exist.
    **infomap_options
        Keyword arguments passed to :class:`infomap.Infomap`. By default,
        ``silent=True`` and ``no_file_output=True`` are used unless
        explicitly overridden.

    Returns
    -------
    igraph.VertexClustering
        The top-level partition, with the codelength of the solution attached
        as a ``codelength`` attribute. For an empty graph, an empty
        clustering with ``codelength`` 0.0 is returned without running
        Infomap.

    Raises
    ------
    ValueError
        If both ``trials`` and ``num_trials`` are passed.
    """
    ig = _validate_igraph_graph(g)
    if trials is not None and "num_trials" in infomap_options:
        raise ValueError("Pass only one of `trials` and `num_trials`.")
    if g.vcount() == 0:
        if module_attribute is not None:
            g.vs[module_attribute] = []
        if flow_attribute is not None:
            g.vs[flow_attribute] = []
        clustering = ig.VertexClustering(g, membership=[])
        clustering.codelength = 0.0
        return clustering

    from .._facade import Infomap

    # `num_trials` is the engine option; `trials` is the convenience alias.
    # Default to 10 when the caller specified neither; a caller-passed
    # `num_trials` (only reachable when `trials` is None) wins via update().
    options = {
        "silent": True,
        "no_file_output": True,
        "num_trials": trials if trials is not None else 10,
    }
    options.update(infomap_options)

    infomap = Infomap(**options)
    node_mapping = add_igraph_graph(
        infomap,
        g,
        edge_weights=edge_weights,
        vertex_weights=vertex_weights,
        node_id=node_id,
        layer_id=layer_id,
        multilayer_inter_intra_format=multilayer_inter_intra_format,
        meta_attribute=meta_attribute,
    )
    infomap.run()

    membership, flows = _membership_and_flows(infomap, g, node_mapping)
    if module_attribute is not None:
        g.vs[module_attribute] = membership
    if flow_attribute is not None:
        g.vs[flow_attribute] = flows

    clustering = ig.VertexClustering(g, membership=membership)
    clustering.codelength = infomap.codelength
    return clustering
