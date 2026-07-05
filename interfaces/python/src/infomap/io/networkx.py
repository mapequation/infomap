from __future__ import annotations

from collections.abc import Mapping
from typing import Any

from ._arrays import apply_node_meta_data, community_node_data

# run_networkx and add_networkx_graph are internal plumbing shared with the
# facade and the docs notebooks; only find_communities is public API.
__all__ = ["find_communities"]


def _label_to_internal_id(labels):
    if not labels:
        return {}
    # Identity map only when EVERY label is an int (bools excluded): a mix like
    # [1, "a"] must enumerate, or "a" would map to itself and reach add_node.
    if all(isinstance(label, int) and not isinstance(label, bool) for label in labels):
        return {label: label for label in labels}
    return {label: index for index, label in enumerate(labels)}


def _stable_unique_labels(labels):
    unique = []
    seen = set()
    for label in labels:
        if label in seen:
            continue
        seen.add(label)
        unique.append(label)
    return unique


def _communities_from_infomap(infomap, node_mapping):
    communities = {}

    for node in community_node_data(infomap):
        # Multilayer/relaxation runs can mint extra leaf state nodes whose ids
        # aren't registered vertices; skip them (mirrors the igraph helper).
        if node.state_id not in node_mapping:
            continue
        original_node = node_mapping[node.state_id]
        communities.setdefault(node.module_id, set()).add(original_node)

    return list(communities.values())


def _set_networkx_node_attributes(
    g, infomap, node_mapping, module_attribute, flow_attribute
):
    if module_attribute is None and flow_attribute is None:
        return

    for node in community_node_data(infomap):
        if node.state_id not in node_mapping:
            continue
        original_node = node_mapping[node.state_id]
        if module_attribute is not None:
            g.nodes[original_node][module_attribute] = node.module_id
        if flow_attribute is not None:
            g.nodes[original_node][flow_attribute] = node.flow


def _to_internal_partition(initial_partition, node_mapping):
    if initial_partition is None or not isinstance(initial_partition, Mapping):
        return initial_partition

    internal_by_label = {label: node_id for node_id, label in node_mapping.items()}
    return {
        internal_by_label.get(node_id, node_id): module_id
        for node_id, module_id in initial_partition.items()
    }


def run_networkx(
    g: Any,
    *,
    weight: str | None = "weight",
    node_id: str = "node_id",
    layer_id: str = "layer_id",
    multilayer_inter_intra_format: bool = True,
    initial_partition: Any = None,
    meta_attribute: str | None = None,
    **infomap_options: Any,
) -> tuple[Any, dict[int, Any]]:
    from .._facade import Infomap

    options = {"silent": True, "no_file_output": True}
    options.update(infomap_options)

    infomap = Infomap(**options)
    node_mapping = add_networkx_graph(
        infomap,
        g,
        weight=weight,
        node_id=node_id,
        layer_id=layer_id,
        multilayer_inter_intra_format=multilayer_inter_intra_format,
        meta_attribute=meta_attribute,
    )
    infomap.run(
        initial_partition=_to_internal_partition(initial_partition, node_mapping)
    )
    return infomap, node_mapping


def find_communities(
    g: Any,
    *,
    weight: str | None = "weight",
    node_id: str = "node_id",
    layer_id: str = "layer_id",
    multilayer_inter_intra_format: bool = True,
    initial_partition: Any = None,
    module_attribute: str | None = None,
    flow_attribute: str | None = None,
    meta_attribute: str | None = None,
    **infomap_options: Any,
) -> list[set[Any]]:
    """Find communities in a NetworkX-style graph.

    This helper is duck-typed and does not import NetworkX. It accepts the same
    graph objects as :meth:`Infomap.add_networkx_graph`, runs Infomap, and
    returns communities using the original graph node labels.

    Parameters
    ----------
    g : nx.Graph
        A NetworkX-compatible graph.
    weight : str or None, optional
        Key to look up link weight in edge data if present. Default
        ``"weight"``. Use ``None`` to treat every edge as weight 1.
    node_id : str, optional
        Node attribute for physical node ids, implying a state network.
    layer_id : str, optional
        Node attribute for layer ids, implying a multilayer network.
    multilayer_inter_intra_format : bool, optional
        Use intra/inter format to simulate inter-layer links. Default
        ``True``.
    module_attribute : str, optional
        If set, write each node's module id back to this node attribute on
        ``g``.
    flow_attribute : str, optional
        If set, write each node's flow back to this node attribute on ``g``.
    meta_attribute : str, optional
        Node attribute to read categorical metadata from, for use with the
        meta-data map equation. Values are encoded to integers in first-seen
        order and set as Infomap metadata; nodes with missing values are
        skipped. Raises :class:`ValueError` if the attribute is not set on
        any node.
    initial_partition : mapping, optional
        Initial module assignment passed to :meth:`infomap.Infomap.run`.
        Keys may use the original NetworkX node labels.
    **infomap_options
        Keyword arguments passed to :class:`infomap.Infomap`. By default,
        ``silent=True`` and ``no_file_output=True`` are used unless explicitly
        overridden.

    Returns
    -------
    list of set
        A partition of ``g.nodes`` grouped by top-level Infomap module.
    """
    if len(g.nodes) == 0:
        return []

    infomap, node_mapping = run_networkx(
        g,
        weight=weight,
        node_id=node_id,
        layer_id=layer_id,
        multilayer_inter_intra_format=multilayer_inter_intra_format,
        initial_partition=initial_partition,
        meta_attribute=meta_attribute,
        **infomap_options,
    )

    _set_networkx_node_attributes(
        g,
        infomap,
        node_mapping,
        module_attribute,
        flow_attribute,
    )
    return _communities_from_infomap(infomap, node_mapping)


def add_networkx_graph(
    infomap: Any,
    g: Any,
    *,
    weight: str | None = "weight",
    node_id: str = "node_id",
    layer_id: str = "layer_id",
    multilayer_inter_intra_format: bool = True,
    meta_attribute: str | None = None,
) -> dict[int, Any]:
    """Add a NetworkX-style graph to an Infomap instance.

    Directedness
    ------------
    Auto-detected from the graph: when ``g.is_directed()`` is true (e.g. an
    ``nx.DiGraph``) and no flow model has been set on the instance, the
    ``directed`` flag is enabled. (The graph-library adapters diverge here:
    networkx and igraph auto-detect via ``is_directed()``; ``add_scipy_sparse_matrix``
    defaults ``directed=False``; ``add_edge_index`` defaults ``directed=True``.)

    Weight parameter
    ----------------
    This adapter names its edge-weight parameter ``weight`` (the edge-data key
    to read; ``None`` treats every edge as weight 1). The other adapters use
    different names: igraph ``edge_weights``, scipy ``weighted`` (a bool),
    edge_index ``edge_weight``.

    MultiGraph / self-loops
    -----------------------
    Parallel edges from an ``nx.MultiGraph``/``nx.MultiDiGraph`` are each
    forwarded to ``add_link`` (their weights accumulate in the engine), and
    self-loops (``g.add_edge(n, n)``) are passed through to ``add_link`` as-is.
    """
    try:
        nodes = list(g.nodes)
        first = nodes[0]
    except IndexError:
        return {}

    if not infomap._core.flowModelIsSet and g.is_directed():
        infomap._core.setDirected(True)

    node_map = _label_to_internal_id(nodes)
    is_string_id = isinstance(first, str)
    node_ids = dict(g.nodes.data(node_id))
    layer_ids = dict(g.nodes.data(layer_id))
    is_state_network = None not in node_ids.values()
    is_multilayer_network = None not in layer_ids.values()

    if is_multilayer_network and not is_state_network:
        raise ValueError(
            f"A multilayer network (every node carries a {layer_id!r} attribute) "
            f"also requires a {node_id!r} attribute on every node to identify the "
            f"physical node each state node maps to; some nodes are missing it."
        )

    phys_map = {}
    if is_state_network:
        phys_labels = [node_ids[node] for node in nodes]
        phys_map = _label_to_internal_id(_stable_unique_labels(phys_labels))

        if not isinstance(phys_labels[0], int):
            for label, _node_id in phys_map.items():
                infomap.set_name(_node_id, str(label))
        else:
            for label in phys_map:
                infomap.set_name(label, f"{label}")

        if is_multilayer_network:
            for state_label, data in g.nodes.data():
                infomap.network.add_multilayer_node(
                    node_map[state_label],
                    data[layer_id],
                    phys_map[data[node_id]],
                    1.0,
                )
        else:
            for state_label in nodes:
                infomap.add_state_node(
                    node_map[state_label], phys_map[node_ids[state_label]]
                )
    else:
        for node, name in g.nodes.data("name"):
            _node_id = node_map[node]
            node_name = node if is_string_id else name
            infomap.add_node(_node_id, name=node_name)

    if is_multilayer_network:
        for source, target, data in g.edges.data():
            source_state_id = node_map[source]
            target_state_id = node_map[target]
            source_layer_id = layer_ids[source]
            target_layer_id = layer_ids[target]
            source_node_id = phys_map[node_ids[source]]
            target_node_id = phys_map[node_ids[target]]
            edge_weight = data[weight] if weight is not None and weight in data else 1.0

            if multilayer_inter_intra_format:
                if source_layer_id == target_layer_id:
                    infomap.add_multilayer_intra_link(
                        source_layer_id,
                        source_node_id,
                        target_node_id,
                        edge_weight,
                    )
                else:
                    if source_node_id != target_node_id:
                        raise RuntimeError(
                            "Multilayer intra/inter format does not support 'diagonal' links. Use `multilayer_inter_intra_format=False`"
                        )
                    infomap.add_multilayer_inter_link(
                        source_layer_id,
                        source_node_id,
                        target_layer_id,
                        edge_weight,
                    )
            else:
                infomap.network.add_multilayer_state_link(
                    source_state_id,
                    source_layer_id,
                    source_node_id,
                    target_state_id,
                    target_layer_id,
                    target_node_id,
                    edge_weight,
                )
    else:
        for source, target, data in g.edges.data():
            edge_weight = data[weight] if weight is not None and weight in data else 1.0
            infomap.add_link(node_map[source], node_map[target], edge_weight)

    if meta_attribute is not None:
        encoding = apply_node_meta_data(
            infomap,
            ((node_map[node], value) for node, value in g.nodes.data(meta_attribute)),
        )
        if not encoding:
            raise ValueError(
                f"`meta_attribute` {meta_attribute!r} is not set on any node."
            )

    return {node_id: label for label, node_id in node_map.items()}
