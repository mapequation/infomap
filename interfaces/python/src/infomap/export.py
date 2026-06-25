from __future__ import annotations

import warnings
from collections.abc import Mapping
from pathlib import Path
from typing import Any


_DEFAULT_MODULE_ATTRIBUTE = "infomap_module"
_DEFAULT_PATH_ATTRIBUTE = "infomap_path"


def _import_networkx() -> Any:
    try:
        import networkx as nx
    except ImportError as exc:
        raise ImportError(
            "The 'networkx' package is required for NetworkX export support. "
            'Install it with `python -m pip install "infomap[networkx]"`.'
        ) from exc
    return nx


def _import_igraph() -> Any:
    try:
        import igraph as ig
    except ImportError as exc:
        raise ImportError(
            "The 'igraph' package is required for igraph export support. "
            'Install it with `python -m pip install "infomap[igraph]"`.'
        ) from exc
    return ig


def _require_modules(infomap: Any) -> None:
    if not infomap._core.haveModules():
        raise ValueError(
            "Infomap results are not available. Run Infomap before exporting."
        )


def _string_attributes(
    path: tuple[int, ...],
    *,
    module_attribute: str | None,
    path_attribute: str | None,
    include_hierarchy: bool,
    flow: Any = None,
    flow_attribute: str | None = None,
) -> dict[str, str]:
    attributes = {}
    if module_attribute is not None:
        attributes[module_attribute] = str(path[0])
    if include_hierarchy:
        if path_attribute is not None:
            attributes[path_attribute] = ":".join(str(module_id) for module_id in path)
        for level, module_id in enumerate(path, start=1):
            attributes[f"infomap_level_{level}"] = str(module_id)
    if flow_attribute is not None and flow is not None:
        attributes[flow_attribute] = str(flow)
    return attributes


def _flow_by_state_id(infomap: Any) -> dict[int, float]:
    return {node.state_id: node.flow for node in infomap.nodes}


def _mapped_assignments(
    infomap: Any,
    node_mapping: Mapping[Any, Any] | None,
) -> dict[Any, tuple[int, ...]]:
    assignments = infomap.get_multilevel_modules(states=True)
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
    warning_stacklevel: int,
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
            stacklevel=warning_stacklevel,
        )
    return graph_nodes & assignment_nodes


def _is_igraph_graph(graph: Any) -> bool:
    try:
        ig = _import_igraph()
    except ImportError:
        return False
    return isinstance(graph, ig.Graph)


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
    warning_stacklevel: int,
) -> Any:
    _require_modules(im)

    output_graph = graph.copy() if copy else graph
    assignments = _mapped_assignments(im, node_mapping)
    flows = _mapped_flows(im, node_mapping) if flow_attribute is not None else {}
    graph_nodes = set(output_graph.nodes)
    matching_nodes = _validate_node_sets(
        graph_nodes,
        set(assignments),
        strict=strict,
        warning_stacklevel=warning_stacklevel,
    )

    for node in matching_nodes:
        output_graph.nodes[node].update(
            _string_attributes(
                assignments[node],
                module_attribute=module_attribute,
                path_attribute=path_attribute,
                include_hierarchy=include_hierarchy,
                flow=flows.get(node),
                flow_attribute=flow_attribute,
            )
        )

    return output_graph


def annotate_networkx_graph(
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
    """Return a NetworkX graph annotated with Infomap result attributes."""
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
        warning_stacklevel=3,
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
    warning_stacklevel: int,
) -> Any:
    ig = _import_igraph()
    if not isinstance(graph, ig.Graph):
        raise TypeError("`graph` must be an igraph.Graph.")
    _require_modules(im)

    output_graph = graph.copy() if copy else graph
    n_vertices = output_graph.vcount()
    assignments = im.get_multilevel_modules(states=True)
    flows = _flow_by_state_id(im) if flow_attribute is not None else {}
    graph_nodes = set(range(n_vertices))
    matching_nodes = _validate_node_sets(
        graph_nodes,
        set(assignments),
        strict=strict,
        warning_stacklevel=warning_stacklevel,
    )

    values_by_attribute: dict[str, list[str | None]] = {}
    for vertex_id in matching_nodes:
        attributes = _string_attributes(
            assignments[vertex_id],
            module_attribute=module_attribute,
            path_attribute=path_attribute,
            include_hierarchy=include_hierarchy,
            flow=flows.get(vertex_id),
            flow_attribute=flow_attribute,
        )
        for attribute, value in attributes.items():
            values_by_attribute.setdefault(attribute, [None] * n_vertices)
            values_by_attribute[attribute][vertex_id] = value

    for attribute, values in values_by_attribute.items():
        output_graph.vs[attribute] = values

    return output_graph


def annotate_igraph_graph(
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
    """Return a python-igraph graph annotated with Infomap result attributes."""
    return _annotate_igraph_graph(
        graph,
        im,
        module_attribute=module_attribute,
        path_attribute=path_attribute,
        include_hierarchy=include_hierarchy,
        flow_attribute=flow_attribute,
        copy=copy,
        strict=strict,
        warning_stacklevel=3,
    )


def write_graphml(graph: Any, im: Any, path: str | Path, **options: Any) -> None:
    """Write a GraphML file with Infomap result attributes on nodes."""
    if _is_igraph_graph(graph):
        writer_options = {}
        writer_options.update(options.pop("writer_options", {}))
        annotated_graph = _annotate_igraph_graph(
            graph,
            im,
            warning_stacklevel=4,
            **options,
        )
        annotated_graph.write_graphml(str(path), **writer_options)
        return

    nx = _import_networkx()
    writer_options = {}
    writer_options.update(options.pop("writer_options", {}))
    annotated_graph = _annotate_networkx_graph(
        graph,
        im,
        warning_stacklevel=4,
        **options,
    )
    nx.write_graphml(annotated_graph, path, **writer_options)


def write_gexf(graph: Any, im: Any, path: str | Path, **options: Any) -> None:
    """Write a GEXF file with Infomap result attributes on NetworkX nodes."""
    if _is_igraph_graph(graph):
        raise NotImplementedError(
            "GEXF export for igraph graphs is not supported because "
            "python-igraph does not provide a native GEXF writer. Use "
            "`write_graphml()` for igraph graphs or pass a NetworkX graph."
        )

    nx = _import_networkx()
    writer_options = {}
    writer_options.update(options.pop("writer_options", {}))
    annotated_graph = _annotate_networkx_graph(
        graph,
        im,
        warning_stacklevel=4,
        **options,
    )
    nx.write_gexf(annotated_graph, path, **writer_options)
