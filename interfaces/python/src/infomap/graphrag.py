from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

__all__ = [
    "GraphRAGGraph",
    "GraphRAGRunResult",
    "read_graphrag",
    "run_graphrag_communities",
    "write_graphrag_communities",
]


@dataclass
class GraphRAGGraph:
    """GraphRAG-style tables converted to Infomap node ids."""

    entities: Any
    relationships: Any
    sources: Any
    targets: Any
    weights: Any | None
    entity_id_to_node_id: dict[Any, int]
    node_id_to_entity_id: dict[int, Any]
    node_id_to_entity_title: dict[int, Any]
    endpoint_to_node_id: dict[Any, int]
    entity_title_to_node_id: dict[Any, int]
    relationship_ids: list[Any]


@dataclass
class GraphRAGRunResult:
    """Result object returned by :func:`run_graphrag_communities`."""

    infomap: Any
    graph: GraphRAGGraph
    output_dir: Path


def _import_parquet_stack():
    try:
        import pandas as pd
        import pyarrow  # noqa: F401
    except ImportError as err:
        raise ImportError(
            "GraphRAG Parquet support requires pandas and pyarrow. "
            'Install them with `python -m pip install "infomap[graphrag]"`.'
        ) from err
    return pd


def _read_table(table_or_path):
    pd = _import_parquet_stack()
    if isinstance(table_or_path, str | Path):
        return pd.read_parquet(table_or_path)
    return table_or_path


def _require_columns(table, columns):
    table_columns = set(table.columns)
    for column in columns:
        if column is not None and column not in table_columns:
            raise ValueError(f"Missing GraphRAG table column {column!r}.")


def _is_missing(value):
    pd = _import_parquet_stack()
    return bool(pd.isna(value))


def _validate_unique_titles(entities_table, entity_title_col):
    titles = entities_table[entity_title_col]
    if bool(titles.isna().any()):
        raise ValueError(
            f"GraphRAG entity title column {entity_title_col!r} cannot contain "
            "missing values when endpoint_col='title'."
        )
    if bool(titles.duplicated().any()):
        raise ValueError(
            f"GraphRAG entity title column {entity_title_col!r} must be unique "
            "when endpoint_col='title'. Use endpoint_col='id' for id-based "
            "relationship endpoints."
        )


def _validate_relationship_endpoints(relationships_table, source_col, target_col):
    missing_columns = [
        column
        for column in (source_col, target_col)
        if bool(relationships_table[column].isna().any())
    ]
    if missing_columns:
        raise ValueError(
            "GraphRAG relationships contain missing relationship endpoints in "
            f"columns: {', '.join(missing_columns)}."
        )


def _register_node(
    *,
    entity_id,
    entity_title,
    entity_id_to_node_id,
    node_id_to_entity_id,
    node_id_to_entity_title,
    entity_title_to_node_id,
):
    if entity_id in entity_id_to_node_id:
        return entity_id_to_node_id[entity_id]

    node_id = len(node_id_to_entity_id) + 1
    entity_id_to_node_id[entity_id] = node_id
    node_id_to_entity_id[node_id] = entity_id
    node_id_to_entity_title[node_id] = entity_title
    if not _is_missing(entity_title):
        entity_title_to_node_id.setdefault(entity_title, node_id)
    return node_id


def read_graphrag(
    entities,
    relationships,
    *,
    entity_id_col="id",
    entity_title_col="title",
    source_col="source",
    target_col="target",
    weight_col="weight",
    relationship_id_col="id",
    endpoint_col="title",
) -> GraphRAGGraph:
    """Read GraphRAG-style entity and relationship Parquet tables."""
    pd = _import_parquet_stack()
    entities_table = _read_table(entities)
    relationships_table = _read_table(relationships)
    _require_columns(entities_table, [entity_id_col, entity_title_col])
    _require_columns(relationships_table, [source_col, target_col, weight_col])

    entity_id_to_node_id = {}
    node_id_to_entity_id = {}
    node_id_to_entity_title = {}
    entity_title_to_node_id = {}

    if endpoint_col == "title":
        _validate_unique_titles(entities_table, entity_title_col)

    for entity_id, entity_title in zip(
        entities_table[entity_id_col],
        entities_table[entity_title_col],
        strict=True,
    ):
        _register_node(
            entity_id=entity_id,
            entity_title=entity_title,
            entity_id_to_node_id=entity_id_to_node_id,
            node_id_to_entity_id=node_id_to_entity_id,
            node_id_to_entity_title=node_id_to_entity_title,
            entity_title_to_node_id=entity_title_to_node_id,
        )

    if endpoint_col == "id":
        endpoint_to_node_id = entity_id_to_node_id.copy()
    elif endpoint_col == "title":
        endpoint_to_node_id = entity_title_to_node_id.copy()
    else:
        raise ValueError("GraphRAG endpoint_col must be 'id' or 'title'.")

    _validate_relationship_endpoints(relationships_table, source_col, target_col)
    endpoints = pd.concat(
        [relationships_table[source_col], relationships_table[target_col]],
        ignore_index=True,
    ).drop_duplicates()
    for endpoint in endpoints:
        if endpoint not in endpoint_to_node_id:
            _register_node(
                entity_id=endpoint,
                entity_title=endpoint,
                entity_id_to_node_id=entity_id_to_node_id,
                node_id_to_entity_id=node_id_to_entity_id,
                node_id_to_entity_title=node_id_to_entity_title,
                entity_title_to_node_id=entity_title_to_node_id,
            )
            endpoint_to_node_id[endpoint] = entity_id_to_node_id[endpoint]

    sources = (
        relationships_table[source_col]
        .map(endpoint_to_node_id)
        .to_numpy(dtype="uint64")
    )
    targets = (
        relationships_table[target_col]
        .map(endpoint_to_node_id)
        .to_numpy(dtype="uint64")
    )
    weights = None
    if weight_col is not None:
        weights = pd.to_numeric(relationships_table[weight_col]).to_numpy(
            dtype="float64"
        )

    relationship_ids = list(range(len(relationships_table)))
    if (
        relationship_id_col is not None
        and relationship_id_col in relationships_table.columns
    ):
        relationship_ids = relationships_table[relationship_id_col].tolist()

    return GraphRAGGraph(
        entities=entities_table,
        relationships=relationships_table,
        sources=sources,
        targets=targets,
        weights=weights,
        entity_id_to_node_id=entity_id_to_node_id,
        node_id_to_entity_id=node_id_to_entity_id,
        node_id_to_entity_title=node_id_to_entity_title,
        endpoint_to_node_id=endpoint_to_node_id,
        entity_title_to_node_id=entity_title_to_node_id,
        relationship_ids=relationship_ids,
    )


def _output_paths(output):
    output_path = Path(output)
    if output_path.suffix.lower() == ".parquet":
        output_dir = output_path.parent
        communities_path = output_path
    else:
        output_dir = output_path
        communities_path = output_dir / "communities.parquet"

    return {
        "dir": output_dir,
        "nodes": output_dir / "infomap_nodes.parquet",
        "run": output_dir / "infomap_run.json",
        "communities": communities_path,
    }


def _nodes_table(im, graph: GraphRAGGraph):
    multilevel_modules = im.get_multilevel_modules()
    nodes = im.to_dataframe(
        columns=["node_id", "module_id", "flow", "path", "name"],
        sort=True,
    )
    nodes["node_id"] = nodes["node_id"].map(int)
    nodes["entity_id"] = nodes["node_id"].map(graph.node_id_to_entity_id)
    nodes["entity_title"] = nodes["node_id"].map(graph.node_id_to_entity_title)
    nodes["module_path"] = nodes["node_id"].map(
        lambda node_id: [int(module_id) for module_id in multilevel_modules[node_id]]
    )
    nodes["level"] = nodes["module_path"].map(len)
    return nodes[
        [
            "node_id",
            "entity_id",
            "entity_title",
            "module_id",
            "module_path",
            "level",
            "flow",
        ]
    ]


def _community_prefixes(nodes):
    prefixes = set()
    for module_path in nodes["module_path"]:
        module_path = tuple(module_path)
        prefixes.update(module_path[:level] for level in range(1, len(module_path) + 1))
    return sorted(prefixes)


def _prefix_string(prefix):
    return ".".join(str(module_id) for module_id in prefix)


def _entity_ids_by_prefix(nodes):
    entity_ids_by_prefix: dict[tuple[int, ...], list[Any]] = {}
    for row in nodes[["entity_id", "module_path"]].itertuples(index=False):
        module_path = tuple(row.module_path)
        for level in range(1, len(module_path) + 1):
            entity_ids_by_prefix.setdefault(module_path[:level], []).append(
                row.entity_id
            )
    return entity_ids_by_prefix


def _relationship_ids_by_prefix(nodes, graph: GraphRAGGraph):
    node_to_path = dict(
        zip(
            nodes["node_id"].map(int),
            nodes["module_path"].map(tuple),
            strict=True,
        )
    )
    relationship_ids_by_prefix: dict[tuple[int, ...], list[Any]] = {}
    for source, target, relationship_id in zip(
        graph.sources,
        graph.targets,
        graph.relationship_ids,
        strict=True,
    ):
        source_path = node_to_path.get(int(source), ())
        target_path = node_to_path.get(int(target), ())
        common_depth = 0
        for source_module, target_module in zip(source_path, target_path, strict=False):
            if source_module != target_module:
                break
            common_depth += 1
        for level in range(1, common_depth + 1):
            relationship_ids_by_prefix.setdefault(source_path[:level], []).append(
                relationship_id
            )
    return relationship_ids_by_prefix


def _communities_table(nodes, graph: GraphRAGGraph):
    pd = _import_parquet_stack()
    rows = []
    prefixes = _community_prefixes(nodes)
    prefix_to_community = {
        prefix: community_id for community_id, prefix in enumerate(prefixes, start=1)
    }
    children_by_prefix: dict[tuple[int, ...], list[int]] = {}
    for prefix in prefixes:
        if len(prefix) > 1:
            children_by_prefix.setdefault(prefix[:-1], []).append(
                prefix_to_community[prefix]
            )
    entity_ids_by_prefix = _entity_ids_by_prefix(nodes)
    relationship_ids_by_prefix = _relationship_ids_by_prefix(nodes, graph)

    for prefix in prefixes:
        community_id = prefix_to_community[prefix]
        prefix_label = _prefix_string(prefix)
        parent = prefix_to_community.get(prefix[:-1]) if len(prefix) > 1 else None
        entity_ids = entity_ids_by_prefix[prefix]
        rows.append(
            {
                "id": f"infomap-{prefix_label}",
                "human_readable_id": prefix_label,
                "community": community_id,
                "parent": parent,
                "children": children_by_prefix.get(prefix, []),
                "level": len(prefix),
                "title": f"Infomap community {prefix_label}",
                "entity_ids": entity_ids,
                "relationship_ids": relationship_ids_by_prefix.get(prefix, []),
                "text_unit_ids": [],
                "period": None,
                "size": len(entity_ids),
            }
        )
    return pd.DataFrame(
        rows,
        columns=[
            "id",
            "human_readable_id",
            "community",
            "parent",
            "children",
            "level",
            "title",
            "entity_ids",
            "relationship_ids",
            "text_unit_ids",
            "period",
            "size",
        ],
    )


def _run_summary_metadata(im):
    codelengths = list(im.codelengths)
    best_trial = codelengths.index(min(codelengths)) + 1 if codelengths else 0
    return {
        "version": getattr(im, "version", ""),
        "codelength": im.codelength,
        "top_modules": im.num_top_modules,
        "levels": im.max_depth,
        "trials": len(codelengths),
        "best_trial": best_trial,
        "trial_codelengths": codelengths,
        "trial_top_modules": [],
    }


def _links_array(graph: GraphRAGGraph):
    import numpy as np

    columns = [graph.sources, graph.targets]
    if graph.weights is not None:
        columns.append(graph.weights)
    return np.column_stack(columns)


def write_graphrag_communities(im, *, graph: GraphRAGGraph, output):
    """Write experimental GraphRAG-compatible community tables."""
    _import_parquet_stack()
    paths = _output_paths(output)
    paths["dir"].mkdir(parents=True, exist_ok=True)

    nodes = _nodes_table(im, graph)
    communities = _communities_table(nodes, graph)

    nodes.to_parquet(paths["nodes"], index=False)
    communities.to_parquet(paths["communities"], index=False)
    if not paths["run"].exists():
        paths["run"].write_text(json.dumps(_run_summary_metadata(im), indent=2) + "\n")


def run_graphrag_communities(
    *,
    input_dir,
    output_dir,
    options="--silent",
    entities_name="entities.parquet",
    relationships_name="relationships.parquet",
    **read_options,
) -> GraphRAGRunResult:
    """Read GraphRAG tables, run Infomap, and write MVP community outputs."""
    from infomap import Infomap

    input_path = Path(input_dir)
    graph = read_graphrag(
        input_path / entities_name,
        input_path / relationships_name,
        **read_options,
    )
    paths = _output_paths(output_dir)
    paths["dir"].mkdir(parents=True, exist_ok=True)

    im = Infomap(options, summary_json=str(paths["run"]))
    im.add_links(_links_array(graph))
    im.run()
    write_graphrag_communities(im, graph=graph, output=output_dir)
    return GraphRAGRunResult(infomap=im, graph=graph, output_dir=Path(output_dir))
