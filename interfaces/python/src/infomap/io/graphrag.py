from __future__ import annotations

import json
from dataclasses import dataclass
from itertools import chain
from pathlib import Path
from typing import Any


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

    for _, entity in entities_table.iterrows():
        _register_node(
            entity_id=entity[entity_id_col],
            entity_title=entity[entity_title_col],
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

    endpoints = chain(relationships_table[source_col], relationships_table[target_col])
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
    if output_path.suffix:
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
    nodes = im.to_dataframe(
        columns=["node_id", "module_id", "flow", "path", "name"],
        sort=True,
    )
    nodes["node_id"] = nodes["node_id"].map(int)
    nodes["entity_id"] = nodes["node_id"].map(graph.node_id_to_entity_id)
    nodes["entity_title"] = nodes["node_id"].map(graph.node_id_to_entity_title)
    nodes["module_path"] = nodes["path"].map(lambda path: list(path))
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


def _relationship_ids_for_nodes(graph: GraphRAGGraph, node_ids: set[int]):
    relationship_ids = []
    for source, target, relationship_id in zip(
        graph.sources,
        graph.targets,
        graph.relationship_ids,
        strict=True,
    ):
        if int(source) in node_ids and int(target) in node_ids:
            relationship_ids.append(relationship_id)
    return relationship_ids


def _communities_table(nodes, graph: GraphRAGGraph):
    pd = _import_parquet_stack()
    rows = []
    for module_id, module_nodes in nodes.groupby("module_id", sort=True):
        entity_ids = module_nodes["entity_id"].tolist()
        node_ids = set(module_nodes["node_id"].map(int))
        rows.append(
            {
                "id": f"infomap-{module_id}",
                "human_readable_id": str(module_id),
                "community": int(module_id),
                "parent": None,
                "children": [],
                "level": 1,
                "title": f"Infomap community {module_id}",
                "entity_ids": entity_ids,
                "relationship_ids": _relationship_ids_for_nodes(graph, node_ids),
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


def _option_value(options: str, flag: str):
    parts = options.split()
    try:
        return int(parts[parts.index(flag) + 1])
    except (ValueError, IndexError):
        return None


def _run_metadata(im):
    options = " ".join(getattr(im, "_last_run_args", getattr(im, "_args", "")).split())
    return {
        "options": options,
        "num_nodes": im.num_nodes,
        "num_links": im.num_links,
        "codelength": im.codelength,
        "num_top_modules": im.num_top_modules,
        "max_depth": im.max_depth,
        "seed": _option_value(options, "--seed"),
    }


def write_graphrag_communities(im, *, graph: GraphRAGGraph, output):
    """Write experimental GraphRAG-compatible community tables."""
    _import_parquet_stack()
    paths = _output_paths(output)
    paths["dir"].mkdir(parents=True, exist_ok=True)

    nodes = _nodes_table(im, graph)
    communities = _communities_table(nodes, graph)

    nodes.to_parquet(paths["nodes"], index=False)
    communities.to_parquet(paths["communities"], index=False)
    paths["run"].write_text(json.dumps(_run_metadata(im), indent=2) + "\n")


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
    im = Infomap(options)
    im.add_links(graph.sources, graph.targets, graph.weights)
    im.run()
    write_graphrag_communities(im, graph=graph, output=output_dir)
    return GraphRAGRunResult(infomap=im, graph=graph, output_dir=Path(output_dir))
