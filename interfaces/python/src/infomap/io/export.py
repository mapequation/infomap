from __future__ import annotations

import os
import warnings
from collections.abc import Mapping
from typing import TYPE_CHECKING, Any

from .._optional import require_igraph, require_networkx
from .._options import _external_stacklevel
from ._arrays import require_modules as _require_modules

if TYPE_CHECKING:
    import igraph  # pyright: ignore[reportMissingImports]  # optional dep, no stubs
    import networkx

    from .._facade import Infomap
    from ..result import Result


__all__ = [
    "annotate_igraph_graph",
    "annotate_networkx_graph",
    "to_igraph",
    "to_networkx",
    "write_gexf",
    "write_graphml",
]

_DEFAULT_MODULE_ATTRIBUTE = "infomap_module"
_DEFAULT_PATH_ATTRIBUTE = "infomap_path"


def _import_networkx() -> Any:
    # Thin delegate; the shared guard lives in infomap._optional.
    return require_networkx("NetworkX export support")


def _import_igraph() -> Any:
    # Thin delegate; the shared guard lives in infomap._optional.
    return require_igraph("igraph export support")


def _node_attributes(
    path: tuple[int, ...],
    *,
    module_attribute: str | None,
    path_attribute: str | None,
    include_hierarchy: bool,
    flow: Any = None,
    flow_attribute: str | None = None,
    stringify: bool = False,
) -> dict[str, Any]:
    # The annotate family stringifies every value (its documented contract);
    # the to_* builders keep native ints and floats. The path is a string
    # either way.
    coerce = str if stringify else (lambda value: value)
    attributes: dict[str, Any] = {}
    if module_attribute is not None:
        attributes[module_attribute] = coerce(path[0])
    if include_hierarchy:
        if path_attribute is not None:
            attributes[path_attribute] = ":".join(str(module_id) for module_id in path)
        for level, module_id in enumerate(path, start=1):
            attributes[f"infomap_level_{level}"] = coerce(module_id)
    if flow_attribute is not None and flow is not None:
        attributes[flow_attribute] = coerce(flow)
    return attributes


def _flow_by_state_id(infomap: Any) -> dict[int, float]:
    # Read through the engine core, not the deprecated Infomap.nodes property.
    node_data = infomap._core.get_node_data(1, True)
    return dict(zip(list(node_data.state_id), list(node_data.flow)))


def _mapped_assignments(
    infomap: Any,
    node_mapping: Mapping[Any, Any] | None,
) -> dict[Any, tuple[int, ...]]:
    assignments = infomap._core.getMultilevelModules(True)
    if node_mapping is None:
        return assignments
    return {
        node_mapping[internal_id]: path
        for internal_id, path in assignments.items()
        if internal_id in node_mapping
    }


def _mapped_flows(
    infomap: Any,
    node_mapping: Mapping[Any, Any] | None,
) -> dict[Any, float]:
    flows = _flow_by_state_id(infomap)
    if node_mapping is None:
        return flows
    return {
        node_mapping[internal_id]: flow
        for internal_id, flow in flows.items()
        if internal_id in node_mapping
    }


def _validate_node_sets(
    graph_nodes: set[Any],
    assignment_nodes: set[Any],
    *,
    strict: bool,
) -> set[Any]:
    missing_assignments = graph_nodes - assignment_nodes
    extra_assignments = assignment_nodes - graph_nodes
    if strict and missing_assignments:
        raise ValueError(
            "The graph contains nodes without Infomap assignments: "
            f"{sorted(missing_assignments, key=repr)!r}."
        )
    if strict and extra_assignments:
        raise ValueError(
            "Infomap assignments contain nodes not present in the graph: "
            f"{sorted(extra_assignments, key=repr)!r}."
        )
    if missing_assignments or extra_assignments:
        warnings.warn(
            "Infomap export annotated only matching graph nodes; "
            f"{len(missing_assignments)} graph nodes had no assignment and "
            f"{len(extra_assignments)} assignments had no graph node.",
            UserWarning,
            stacklevel=_external_stacklevel(),
        )
    return graph_nodes & assignment_nodes


def _is_igraph_graph(graph: Any) -> bool:
    try:
        ig = _import_igraph()
    except ImportError:
        return False
    return isinstance(graph, ig.Graph)


def _resolve_run_source(im_or_result: Any) -> Any:
    """Return an engine-bearing object for the annotate/write helpers.

    Accepts a run :class:`~infomap.Infomap` / :class:`~infomap.Network`
    (returned unchanged) or the :class:`~infomap.Result` it produced, which
    is generation-checked and unwrapped to its engine — so the annotate/write
    helpers share the ``to_networkx``/``to_igraph`` input contract.
    """
    if hasattr(im_or_result, "_check_generation"):  # Result
        im_or_result._check_generation()
        return im_or_result._engine
    return im_or_result


def _annotate_networkx_graph(
    graph: Any,
    im: Any,
    *,
    node_mapping: Mapping[Any, Any] | None = None,
    module_attribute: str | None = _DEFAULT_MODULE_ATTRIBUTE,
    path_attribute: str | None = _DEFAULT_PATH_ATTRIBUTE,
    include_hierarchy: bool = True,
    flow_attribute: str | None = None,
    copy: bool = True,
    strict: bool = True,
) -> Any:
    im = _resolve_run_source(im)
    _require_modules(im)

    output_graph = graph.copy() if copy else graph
    assignments = _mapped_assignments(im, node_mapping)
    flows = _mapped_flows(im, node_mapping) if flow_attribute is not None else {}
    graph_nodes = set(output_graph.nodes)
    matching_nodes = _validate_node_sets(
        graph_nodes,
        set(assignments),
        strict=strict,
    )

    for node in matching_nodes:
        output_graph.nodes[node].update(
            _node_attributes(
                assignments[node],
                module_attribute=module_attribute,
                path_attribute=path_attribute,
                include_hierarchy=include_hierarchy,
                flow=flows.get(node),
                flow_attribute=flow_attribute,
                stringify=True,
            )
        )

    return output_graph


def annotate_networkx_graph(
    graph: networkx.Graph,
    im: Infomap | Result,
    *,
    node_mapping: Mapping[Any, Any] | None = None,
    module_attribute: str | None = _DEFAULT_MODULE_ATTRIBUTE,
    path_attribute: str | None = _DEFAULT_PATH_ATTRIBUTE,
    include_hierarchy: bool = True,
    flow_attribute: str | None = None,
    copy: bool = True,
    strict: bool = True,
) -> networkx.Graph:
    """Return a NetworkX graph annotated with Infomap result attributes.

    Writes each node's module assignment (and optionally its tree path,
    per-level module ids, and flow) as string-valued node attributes, suitable
    for GraphML/GEXF export.

    Parameters
    ----------
    graph : networkx.Graph
        The graph to annotate.
    im : Infomap or Result
        A run :class:`~infomap.Infomap` instance, or the
        :class:`~infomap.Result` it returned (the result's engine is used).
        Raises if :meth:`~infomap.Infomap.run` has not been called.
    node_mapping : mapping, optional
        Mapping from internal Infomap (state) ids to graph node labels, as
        returned by the ``add_*_graph`` adapters. If omitted, graph nodes are
        assumed to be the internal ids.
    module_attribute : str or None, optional
        Node attribute for the top-level module id. Default
        ``"infomap_module"``. Use ``None`` to omit.
    path_attribute : str or None, optional
        Node attribute for the colon-joined tree path. Default
        ``"infomap_path"``. Use ``None`` to omit.
    include_hierarchy : bool, optional
        Also write per-level ``infomap_level_{n}`` attributes (and the path
        attribute). Default ``True``.
    flow_attribute : str or None, optional
        Node attribute for the node flow. Default ``None`` (omitted).
    copy : bool, optional
        Annotate a copy of ``graph`` instead of modifying it in place.
        Default ``True``.
    strict : bool, optional
        Raise :class:`ValueError` when the graph nodes and the Infomap
        assignments do not match exactly. If ``False``, only the matching
        nodes are annotated and a warning is emitted. Default ``True``.

    Returns
    -------
    networkx.Graph
        The annotated graph (a copy when ``copy=True``, otherwise ``graph``).
    """
    return _annotate_networkx_graph(
        graph,
        im,
        node_mapping=node_mapping,
        module_attribute=module_attribute,
        path_attribute=path_attribute,
        include_hierarchy=include_hierarchy,
        flow_attribute=flow_attribute,
        copy=copy,
        strict=strict,
    )


def _annotate_igraph_graph(
    graph: Any,
    im: Any,
    *,
    module_attribute: str | None = _DEFAULT_MODULE_ATTRIBUTE,
    path_attribute: str | None = _DEFAULT_PATH_ATTRIBUTE,
    include_hierarchy: bool = True,
    flow_attribute: str | None = None,
    copy: bool = True,
    strict: bool = True,
) -> Any:
    ig = _import_igraph()
    if not isinstance(graph, ig.Graph):
        raise TypeError("`graph` must be an igraph.Graph.")
    im = _resolve_run_source(im)
    _require_modules(im)

    output_graph = graph.copy() if copy else graph
    n_vertices = output_graph.vcount()
    assignments = im._core.getMultilevelModules(True)
    flows = _flow_by_state_id(im) if flow_attribute is not None else {}
    graph_nodes = set(range(n_vertices))
    matching_nodes = _validate_node_sets(
        graph_nodes,
        set(assignments),
        strict=strict,
    )

    values_by_attribute: dict[str, list[str | None]] = {}
    for vertex_id in matching_nodes:
        attributes = _node_attributes(
            assignments[vertex_id],
            module_attribute=module_attribute,
            path_attribute=path_attribute,
            include_hierarchy=include_hierarchy,
            flow=flows.get(vertex_id),
            flow_attribute=flow_attribute,
            stringify=True,
        )
        for attribute, value in attributes.items():
            values_by_attribute.setdefault(attribute, [None] * n_vertices)
            values_by_attribute[attribute][vertex_id] = value

    for attribute, values in values_by_attribute.items():
        output_graph.vs[attribute] = values

    return output_graph


def annotate_igraph_graph(
    graph: igraph.Graph,
    im: Infomap | Result,
    *,
    module_attribute: str | None = _DEFAULT_MODULE_ATTRIBUTE,
    path_attribute: str | None = _DEFAULT_PATH_ATTRIBUTE,
    include_hierarchy: bool = True,
    flow_attribute: str | None = None,
    copy: bool = True,
    strict: bool = True,
) -> igraph.Graph:
    """Return a python-igraph graph annotated with Infomap result attributes.

    Writes each vertex's module assignment (and optionally its tree path,
    per-level module ids, and flow) as string-valued vertex attributes,
    suitable for GraphML export. Vertices are matched to Infomap state ids by
    igraph vertex index, as assigned by :meth:`infomap.Infomap.add_igraph_graph`.

    Parameters
    ----------
    graph : igraph.Graph
        The graph to annotate.
    im : Infomap or Result
        A run :class:`~infomap.Infomap` instance, or the
        :class:`~infomap.Result` it returned (the result's engine is used).
        Raises if :meth:`~infomap.Infomap.run` has not been called.
    module_attribute : str or None, optional
        Vertex attribute for the top-level module id. Default
        ``"infomap_module"``. Use ``None`` to omit.
    path_attribute : str or None, optional
        Vertex attribute for the colon-joined tree path. Default
        ``"infomap_path"``. Use ``None`` to omit.
    include_hierarchy : bool, optional
        Also write per-level ``infomap_level_{n}`` attributes (and the path
        attribute). Default ``True``.
    flow_attribute : str or None, optional
        Vertex attribute for the node flow. Default ``None`` (omitted).
    copy : bool, optional
        Annotate a copy of ``graph`` instead of modifying it in place.
        Default ``True``.
    strict : bool, optional
        Raise :class:`ValueError` when the graph vertices and the Infomap
        assignments do not match exactly. If ``False``, only the matching
        vertices are annotated and a warning is emitted. Default ``True``.

    Returns
    -------
    igraph.Graph
        The annotated graph (a copy when ``copy=True``, otherwise ``graph``).
    """
    return _annotate_igraph_graph(
        graph,
        im,
        module_attribute=module_attribute,
        path_attribute=path_attribute,
        include_hierarchy=include_hierarchy,
        flow_attribute=flow_attribute,
        copy=copy,
        strict=strict,
    )


def _result_nodes(result: Any):
    """Yield the leaf state nodes of a ``Result`` (generation-guarded)."""
    return result.nodes(states=True)


def _result_links(result: Any):
    """Yield ``(source_state_id, target_state_id, weight)`` for a ``Result``.

    Reads through the public, generation-guarded ``Result.links`` accessor.
    """
    return result.links(data="weight")


def to_networkx(
    result: Result,
    *,
    module_attribute: str | None = _DEFAULT_MODULE_ATTRIBUTE,
    path_attribute: str | None = _DEFAULT_PATH_ATTRIBUTE,
    include_hierarchy: bool = True,
    flow_attribute: str | None = "flow",
) -> networkx.Graph:
    """Build a NetworkX graph from a :class:`~infomap.Result`.

    Nodes are the result's (state) nodes, keyed by ``state_id``, carrying the
    Infomap node ``name`` plus the module/path/flow attributes. Edges come
    from the partitioned network, with their weights as ``weight``. The
    attribute values keep their native types: module ids are :class:`int`,
    flows and weights are :class:`float`, and the tree path is a
    colon-joined :class:`str` such as ``"1:3"``.

    Parameters
    ----------
    result : Result
        A result returned by :func:`infomap.run` or :meth:`Infomap.run`.
    module_attribute : str or None, optional
        Node attribute for the top-level module id. Default
        ``"infomap_module"``. Use ``None`` to omit.
    path_attribute : str or None, optional
        Node attribute for the colon-joined tree path. Default
        ``"infomap_path"``. Use ``None`` to omit.
    include_hierarchy : bool, optional
        Also write per-level ``infomap_level_{n}`` attributes. Default
        ``True``.
    flow_attribute : str or None, optional
        Node attribute for the node flow. Default ``"flow"``. Use ``None`` to
        omit.

    Returns
    -------
    networkx.Graph or networkx.DiGraph
        ``DiGraph`` when the partitioned network is directed, else ``Graph``.

    See Also
    --------
    infomap.Result.to_networkx : The same conversion as a ``Result`` method.
    annotate_networkx_graph : Annotate an existing graph in place instead
        (string-valued attributes).
    """
    nx = _import_networkx()
    result._check_generation()

    directed = bool(result._directed)
    graph = nx.DiGraph() if directed else nx.Graph()

    for node in _result_nodes(result):
        attributes = {"name": node.name}
        attributes.update(
            _node_attributes(
                node.path,
                module_attribute=module_attribute,
                path_attribute=path_attribute,
                include_hierarchy=include_hierarchy,
                flow=node.flow,
                flow_attribute=flow_attribute,
            )
        )
        graph.add_node(node.state_id, **attributes)

    for source, target, weight in _result_links(result):
        graph.add_edge(source, target, weight=weight)

    return graph


def to_igraph(
    result: Result,
    *,
    module_attribute: str | None = _DEFAULT_MODULE_ATTRIBUTE,
    path_attribute: str | None = _DEFAULT_PATH_ATTRIBUTE,
    include_hierarchy: bool = True,
    flow_attribute: str | None = "flow",
) -> igraph.Graph:
    """Build a python-igraph graph from a :class:`~infomap.Result`.

    Vertices are the result's (state) nodes in the order the result yields
    them, carrying the Infomap node ``name`` plus the module/path/flow
    attributes. Edges come from the partitioned network, with their weights as
    ``weight``. The attribute values keep their native types, as in
    :func:`to_networkx`.

    Parameters
    ----------
    result : Result
        A result returned by :func:`infomap.run` or :meth:`Infomap.run`.
    module_attribute, path_attribute, include_hierarchy, flow_attribute
        See :func:`to_networkx`.

    Returns
    -------
    igraph.Graph
        Directed when the partitioned network is directed, else undirected.

    See Also
    --------
    infomap.Result.to_igraph : The same conversion as a ``Result`` method.
    annotate_igraph_graph : Annotate an existing graph in place instead
        (string-valued attributes).
    """
    ig = _import_igraph()

    nodes = list(_result_nodes(result))
    state_ids = [node.state_id for node in nodes]
    # igraph vertices are dense 0..n-1; map state ids onto that range.
    index_by_state_id = {state_id: index for index, state_id in enumerate(state_ids)}
    n_vertices = len(nodes)

    directed = bool(result._directed)
    graph = ig.Graph(n=n_vertices, directed=directed)

    values_by_attribute: dict[str, list[Any]] = {"name": [None] * n_vertices}
    for index, node in enumerate(nodes):
        values_by_attribute["name"][index] = node.name
        attributes = _node_attributes(
            node.path,
            module_attribute=module_attribute,
            path_attribute=path_attribute,
            include_hierarchy=include_hierarchy,
            flow=node.flow,
            flow_attribute=flow_attribute,
        )
        for attribute, value in attributes.items():
            values_by_attribute.setdefault(attribute, [None] * n_vertices)
            values_by_attribute[attribute][index] = value

    for attribute, values in values_by_attribute.items():
        graph.vs[attribute] = values

    edges = []
    weights = []
    for source, target, weight in _result_links(result):
        if source in index_by_state_id and target in index_by_state_id:
            edges.append((index_by_state_id[source], index_by_state_id[target]))
            weights.append(weight)
    if edges:
        graph.add_edges(edges)
        graph.es["weight"] = weights

    return graph


def write_graphml(
    graph: networkx.Graph | igraph.Graph,
    im: Infomap | Result,
    path: str | os.PathLike[str],
    **options: Any,
) -> None:
    """Write a GraphML file with Infomap result attributes on nodes.

    Annotates ``graph`` (without modifying it) and writes it as GraphML.
    Accepts both NetworkX and python-igraph graphs; the graph type selects
    the annotate helper and writer.

    Parameters
    ----------
    graph : networkx.Graph or igraph.Graph
        The graph to annotate and write.
    im : Infomap or Result
        A run :class:`~infomap.Infomap` instance, or the
        :class:`~infomap.Result` it returned (the result's engine is used).
    path : str or os.PathLike
        Output file path.
    **options
        Passed through to :func:`annotate_networkx_graph` or
        :func:`annotate_igraph_graph` (e.g. ``module_attribute``,
        ``flow_attribute``, ``strict``). The special key ``writer_options``
        (a dict) is instead passed to the underlying GraphML writer.

    Returns
    -------
    None
    """
    path = os.fsdecode(path)
    if _is_igraph_graph(graph):
        writer_options = {}
        writer_options.update(options.pop("writer_options", {}))
        annotated_graph = _annotate_igraph_graph(
            graph,
            im,
            **options,
        )
        annotated_graph.write_graphml(path, **writer_options)
        return

    nx = _import_networkx()
    writer_options = {}
    writer_options.update(options.pop("writer_options", {}))
    annotated_graph = _annotate_networkx_graph(
        graph,
        im,
        **options,
    )
    nx.write_graphml(annotated_graph, path, **writer_options)


def write_gexf(
    graph: networkx.Graph,
    im: Infomap | Result,
    path: str | os.PathLike[str],
    **options: Any,
) -> None:
    """Write a GEXF file with Infomap result attributes on NetworkX nodes.

    Annotates ``graph`` (without modifying it) and writes it as GEXF via
    NetworkX.

    Parameters
    ----------
    graph : networkx.Graph
        The graph to annotate and write.
    im : Infomap or Result
        A run :class:`~infomap.Infomap` instance, or the
        :class:`~infomap.Result` it returned (the result's engine is used).
    path : str or os.PathLike
        Output file path.
    **options
        Passed through to :func:`annotate_networkx_graph` (e.g.
        ``module_attribute``, ``flow_attribute``, ``strict``). The special
        key ``writer_options`` (a dict) is instead passed to
        ``networkx.write_gexf``.

    Returns
    -------
    None

    Raises
    ------
    NotImplementedError
        If ``graph`` is a python-igraph graph: python-igraph has no native
        GEXF writer. Use :func:`write_graphml` or pass a NetworkX graph.
    """
    if _is_igraph_graph(graph):
        raise NotImplementedError(
            "GEXF export for igraph graphs is not supported because "
            "python-igraph does not provide a native GEXF writer. Use "
            "`write_graphml()` for igraph graphs or pass a NetworkX graph."
        )

    path = os.fsdecode(path)
    nx = _import_networkx()
    writer_options = {}
    writer_options.update(options.pop("writer_options", {}))
    annotated_graph = _annotate_networkx_graph(
        graph,
        im,
        **options,
    )
    nx.write_gexf(annotated_graph, path, **writer_options)
