from __future__ import annotations

import math
from collections.abc import Iterable
from numbers import Integral, Real
from typing import Any


def _import_igraph() -> Any:
    try:
        import igraph as ig
    except ImportError as exc:
        raise ImportError(
            "The 'igraph' package is required for igraph graph support. "
            'Install it with `python -m pip install "infomap[igraph]"`.'
        ) from exc
    return ig


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
    if isinstance(value, Real) and math.isfinite(value) and int(value) == value:
        return int(value)
    raise ValueError(f"`{name}` values must be integer-like.")


def _node_ids(values):
    if all(isinstance(value, Real) and not isinstance(value, bool) for value in values):
        ids = [_integer_like(value, name="node_id") for value in values]
        return ids, {_node_id: str(_node_id) for _node_id in dict.fromkeys(ids)}

    label_to_id = {}
    ids = []
    for label in values:
        if label not in label_to_id:
            label_to_id[label] = len(label_to_id)
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
) -> dict[int, Any]:
    """Add a python-igraph graph to an Infomap instance."""
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
                        raise RuntimeError(
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

    if names is None:
        return {vertex_id: vertex_id for vertex_id in vertices}
    return {vertex_id: names[vertex_id] for vertex_id in vertices}


def _membership_and_flows(infomap, g):
    membership = [None] * g.vcount()
    flows = [None] * g.vcount()
    module_ids = {}

    for node in infomap.nodes:
        vertex_id = node.state_id
        if vertex_id < 0 or vertex_id >= g.vcount():
            continue
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
    trials: int = 10,
    node_id: str = "node_id",
    layer_id: str = "layer_id",
    multilayer_inter_intra_format: bool = True,
    module_attribute: str | None = None,
    flow_attribute: str | None = None,
    **infomap_options: Any,
) -> Any:
    """Find communities in a python-igraph graph."""
    ig = _validate_igraph_graph(g)
    if "num_trials" in infomap_options:
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

    options = {"silent": True, "no_file_output": True, "num_trials": trials}
    options.update(infomap_options)

    infomap = Infomap(**options)
    add_igraph_graph(
        infomap,
        g,
        edge_weights=edge_weights,
        vertex_weights=vertex_weights,
        node_id=node_id,
        layer_id=layer_id,
        multilayer_inter_intra_format=multilayer_inter_intra_format,
    )
    infomap.run()

    membership, flows = _membership_and_flows(infomap, g)
    if module_attribute is not None:
        g.vs[module_attribute] = membership
    if flow_attribute is not None:
        g.vs[flow_attribute] = flows

    clustering = ig.VertexClustering(g, membership=membership)
    clustering.codelength = infomap.codelength
    return clustering
