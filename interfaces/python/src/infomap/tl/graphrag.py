from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any

from ..io._arrays import require_modules as _require_modules

if TYPE_CHECKING:
    import pandas

__all__ = [
    "GraphRAGGraph",
    "GraphRAGRunResult",
    "read_graphrag",
    "run_graphrag_communities",
    "write_graphrag_communities",
]


@dataclass
class GraphRAGGraph:
    """GraphRAG-style tables converted to Infomap node ids.

    The mapping fields are advanced plumbing for callers that need to join
    Infomap node ids back to the original GraphRAG entity and relationship
    tables.
    """

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
    entity_id_col: str


@dataclass
class GraphRAGRunResult:
    """Result object returned by :func:`run_graphrag_communities`."""

    infomap: Any
    #: The immutable :class:`~infomap.Result` from the run. Read run metrics
    #: from here (``result.result.codelength``, ``.num_top_modules``) rather
    #: than the deprecated accessors on the stateful ``infomap`` instance.
    result: Any
    graph: GraphRAGGraph
    output_dir: Path | None
    nodes: Any
    communities: Any


def _import_parquet_stack():
    from .._optional import require_pandas, require_pyarrow

    pd = require_pandas("GraphRAG Parquet support", extra="graphrag")
    require_pyarrow("GraphRAG Parquet support")
    return pd


def _read_table(table_or_path):
    pd = _import_parquet_stack()
    if isinstance(table_or_path, (str, Path)):
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


def _validate_unique_entity_ids(entities_table, entity_id_col):
    entity_ids = entities_table[entity_id_col]
    if bool(entity_ids.isna().any()):
        raise ValueError(
            f"GraphRAG entity id column {entity_id_col!r} cannot contain missing "
            "values."
        )
    if bool(entity_ids.duplicated().any()):
        raise ValueError(f"GraphRAG entity id column {entity_id_col!r} must be unique.")


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
    entities: "str | Path | pandas.DataFrame",
    relationships: "str | Path | pandas.DataFrame",
    *,
    entity_id_col: str = "id",
    entity_title_col: str = "title",
    source_col: str = "source",
    target_col: str = "target",
    weight_col: str = "weight",
    relationship_id_col: str = "id",
    endpoint_col: str = "title",
) -> GraphRAGGraph:
    """Read GraphRAG-style entity and relationship Parquet tables.

    Assigns a 1-based Infomap node id to every entity (and to any relationship
    endpoint that does not appear in the entity table) and converts the
    relationships to source/target/weight arrays ready for
    :meth:`infomap.Infomap.add_links`.

    Parameters
    ----------
    entities : str, pathlib.Path, or pandas.DataFrame
        Entity table, or a path to a Parquet file containing it.
    relationships : str, pathlib.Path, or pandas.DataFrame
        Relationship table, or a path to a Parquet file containing it.
    entity_id_col : str, optional
        Entity table column with unique entity ids. Default ``"id"``.
    entity_title_col : str, optional
        Entity table column with entity titles. Default ``"title"``.
    source_col : str, optional
        Relationship table column with source endpoints. Default ``"source"``.
    target_col : str, optional
        Relationship table column with target endpoints. Default ``"target"``.
    weight_col : str or None, optional
        Relationship table column with link weights, or ``None`` for
        unweighted links. Default ``"weight"``.
    relationship_id_col : str or None, optional
        Relationship table column with relationship ids. If missing or
        ``None``, positional indices are used. Default ``"id"``.
    endpoint_col : str, optional
        Which entity column the relationship endpoints reference: ``"title"``
        (default, requires unique titles) or ``"id"``.

    Returns
    -------
    GraphRAGGraph
        The tables together with the link arrays and the mappings between
        Infomap node ids and entity ids/titles.
    """
    pd = _import_parquet_stack()
    entities_table = _read_table(entities)
    relationships_table = _read_table(relationships)
    _require_columns(entities_table, [entity_id_col, entity_title_col])
    _require_columns(relationships_table, [source_col, target_col, weight_col])

    entity_id_to_node_id = {}
    node_id_to_entity_id = {}
    node_id_to_entity_title = {}
    entity_title_to_node_id = {}

    _validate_unique_entity_ids(entities_table, entity_id_col)
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

    # pandas' own type hints under-model these correct runtime patterns:
    # ``Series.map`` accepts a Mapping, and ``to_numeric(...).to_numpy(dtype=...)``
    # is valid on its Series result. Targeted ignores for this optional-dep
    # (``infomap[graphrag]``) stub gap.
    sources = (
        relationships_table[source_col]
        .map(endpoint_to_node_id)  # pyright: ignore[reportArgumentType]
        .to_numpy(dtype="uint64")
    )
    targets = (
        relationships_table[target_col]
        .map(endpoint_to_node_id)  # pyright: ignore[reportArgumentType]
        .to_numpy(dtype="uint64")
    )
    weights = None
    if weight_col is not None:
        weights = pd.to_numeric(relationships_table[weight_col]).to_numpy(dtype="float64")  # pyright: ignore[reportAttributeAccessIssue, reportArgumentType]

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
        entity_id_col=entity_id_col,
    )


def _output_paths(output):
    if output is None:
        return {
            "dir": None,
            "nodes": None,
            "run": None,
            "communities": None,
        }

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
    result = im._result
    multilevel_modules = result.multilevel_modules()
    # Only node_id/module_id/flow are read below; module_path is derived from
    # multilevel_modules, not the dataframe "path"/"name" columns.
    nodes = result.to_dataframe(
        columns=["node_id", "module_id", "flow"],
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


def _text_unit_values(value):
    pd = _import_parquet_stack()
    if isinstance(value, str):
        return [value]
    if isinstance(value, (list, tuple)):
        return list(value)
    if hasattr(value, "tolist"):
        return value.tolist()
    if value is None:
        return []
    missing = pd.isna(value)
    try:
        if bool(missing):
            return []
    except ValueError:
        # Array-like missing checks are ambiguous; treat the value as concrete.
        return [value]
    return [value]


def _append_unique_text_units(target, values):
    seen = set(target)
    for value in values:
        if value not in seen:
            target.append(value)
            seen.add(value)


def _entity_text_unit_ids_by_prefix(nodes, graph: GraphRAGGraph):
    if "text_unit_ids" not in graph.entities.columns:
        return {}

    entity_text_units = dict(
        zip(
            graph.entities[graph.entity_id_col],
            graph.entities["text_unit_ids"],
            strict=True,
        )
    )
    text_unit_ids_by_prefix: dict[tuple[int, ...], list[Any]] = {}
    for row in nodes[["entity_id", "module_path"]].itertuples(index=False):
        text_unit_ids = _text_unit_values(entity_text_units.get(row.entity_id))
        module_path = tuple(row.module_path)
        for level in range(1, len(module_path) + 1):
            prefix = module_path[:level]
            _append_unique_text_units(
                text_unit_ids_by_prefix.setdefault(prefix, []),
                text_unit_ids,
            )
    return text_unit_ids_by_prefix


def _node_prefixes_table(nodes):
    pd = _import_parquet_stack()
    rows = []
    for row in nodes[["node_id", "module_path"]].itertuples(index=False):
        module_path = tuple(row.module_path)
        rows.extend(
            {
                "node_id": int(row.node_id),
                "prefix": module_path[:level],
                "level": level,
            }
            for level in range(1, len(module_path) + 1)
        )
    return pd.DataFrame(rows, columns=["node_id", "prefix", "level"])


def _relationship_ids_by_prefix(nodes, graph: GraphRAGGraph):
    pd = _import_parquet_stack()
    prefixes = _node_prefixes_table(nodes)
    if prefixes.empty:
        return {}

    relationships = pd.DataFrame(
        {
            "relationship_index": range(len(graph.relationship_ids)),
            "source": graph.sources.astype("int64", copy=False),
            "target": graph.targets.astype("int64", copy=False),
            "relationship_id": graph.relationship_ids,
        }
    )

    source_prefixes = relationships[
        ["relationship_index", "source", "relationship_id"]
    ].merge(prefixes, left_on="source", right_on="node_id", how="inner", sort=False)
    target_prefixes = relationships[["relationship_index", "target"]].merge(
        prefixes, left_on="target", right_on="node_id", how="inner", sort=False
    )
    common_prefixes = source_prefixes.merge(
        target_prefixes[["relationship_index", "prefix", "level"]],
        on=["relationship_index", "prefix", "level"],
        how="inner",
        sort=False,
    ).sort_values("relationship_index", kind="stable")
    return (
        common_prefixes.groupby("prefix", sort=False)["relationship_id"]
        .agg(list)
        .to_dict()
    )


def _relationship_text_unit_ids_by_prefix(nodes, graph: GraphRAGGraph):
    if "text_unit_ids" not in graph.relationships.columns:
        return {}

    pd = _import_parquet_stack()
    prefixes = _node_prefixes_table(nodes)
    if prefixes.empty:
        return {}

    relationships = pd.DataFrame(
        {
            "relationship_index": range(len(graph.relationship_ids)),
            "source": graph.sources.astype("int64", copy=False),
            "target": graph.targets.astype("int64", copy=False),
            "text_unit_ids": graph.relationships["text_unit_ids"],
        }
    )

    source_prefixes = relationships[
        ["relationship_index", "source", "text_unit_ids"]
    ].merge(prefixes, left_on="source", right_on="node_id", how="inner", sort=False)
    target_prefixes = relationships[["relationship_index", "target"]].merge(
        prefixes, left_on="target", right_on="node_id", how="inner", sort=False
    )
    common_prefixes = source_prefixes.merge(
        target_prefixes[["relationship_index", "prefix", "level"]],
        on=["relationship_index", "prefix", "level"],
        how="inner",
        sort=False,
    ).sort_values("relationship_index", kind="stable")

    text_unit_ids_by_prefix: dict[tuple[int, ...], list[Any]] = {}
    for row in common_prefixes[["prefix", "text_unit_ids"]].itertuples(index=False):
        # pandas types ``itertuples`` rows as ``tuple[Any, ...]`` rather than the
        # named tuple produced at runtime; field access is valid (stub gap).
        _append_unique_text_units(
            text_unit_ids_by_prefix.setdefault(row.prefix, []),  # pyright: ignore[reportAttributeAccessIssue]
            _text_unit_values(row.text_unit_ids),  # pyright: ignore[reportAttributeAccessIssue]
        )
    return text_unit_ids_by_prefix


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
    entity_text_unit_ids_by_prefix = _entity_text_unit_ids_by_prefix(nodes, graph)
    relationship_text_unit_ids_by_prefix = _relationship_text_unit_ids_by_prefix(
        nodes, graph
    )

    for prefix in prefixes:
        community_id = prefix_to_community[prefix]
        prefix_label = _prefix_string(prefix)
        parent = prefix_to_community.get(prefix[:-1], -1) if len(prefix) > 1 else -1
        entity_ids = entity_ids_by_prefix[prefix]
        text_unit_ids = []
        _append_unique_text_units(
            text_unit_ids,
            entity_text_unit_ids_by_prefix.get(prefix, []),
        )
        _append_unique_text_units(
            text_unit_ids,
            relationship_text_unit_ids_by_prefix.get(prefix, []),
        )
        rows.append(
            {
                "id": f"infomap-{prefix_label}",
                "human_readable_id": community_id,
                "community": community_id,
                "parent": parent,
                "children": children_by_prefix.get(prefix, []),
                "level": len(prefix) - 1,
                "title": f"Infomap community {prefix_label}",
                "entity_ids": entity_ids,
                "relationship_ids": relationship_ids_by_prefix.get(prefix, []),
                "text_unit_ids": text_unit_ids,
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


def _links_array(graph: GraphRAGGraph):
    import numpy as np

    columns = [graph.sources, graph.targets]
    if graph.weights is not None:
        columns.append(graph.weights)
    return np.column_stack(columns)


def _build_tables(im, graph: GraphRAGGraph):
    _require_modules(im)
    nodes = _nodes_table(im, graph)
    communities = _communities_table(nodes, graph)
    return nodes, communities


def write_graphrag_communities(
    im: Any, *, graph: GraphRAGGraph, output: str | Path
) -> "tuple[pandas.DataFrame, pandas.DataFrame]":
    """Write GraphRAG-compatible community tables from a finished run.

    Parameters
    ----------
    im : Infomap
        An :class:`~infomap.Infomap` instance that has been run on the links
        of ``graph``.
    graph : GraphRAGGraph
        The graph returned by :func:`read_graphrag` for the same network.
    output : str or pathlib.Path
        Output directory, or a ``.parquet`` path for the communities table
        (its parent directory is then used for the nodes table). Writes
        ``infomap_nodes.parquet`` and ``communities.parquet``.

    Returns
    -------
    tuple of (pandas.DataFrame, pandas.DataFrame)
        The ``(nodes, communities)`` tables that were written.

    Raises
    ------
    ValueError
        If ``output`` is ``None``.
    """
    _import_parquet_stack()
    paths = _output_paths(output)
    if paths["dir"] is None:
        raise ValueError("GraphRAG community output path cannot be None.")
    paths["dir"].mkdir(parents=True, exist_ok=True)

    nodes, communities = _build_tables(im, graph)

    nodes.to_parquet(paths["nodes"], index=False)
    communities.to_parquet(paths["communities"], index=False)
    return nodes, communities


def run_graphrag_communities(
    *,
    input_dir: str | Path,
    output_dir: str | Path | None = None,
    args: str | None = None,
    entities_name: str = "entities.parquet",
    relationships_name: str = "relationships.parquet",
    entity_id_col: str = "id",
    entity_title_col: str = "title",
    source_col: str = "source",
    target_col: str = "target",
    weight_col: str = "weight",
    relationship_id_col: str = "id",
    endpoint_col: str = "title",
    silent: bool = True,
    seed: int = 123,
    num_trials: int = 5,
    **infomap_options: Any,
) -> GraphRAGRunResult:
    """Read GraphRAG tables, run Infomap, and write community outputs.

    End-to-end convenience wrapper around :func:`read_graphrag`, an Infomap
    run, and :func:`write_graphrag_communities`.

    Parameters
    ----------
    input_dir : str or pathlib.Path
        Directory containing the entity and relationship Parquet files.
    output_dir : str or pathlib.Path, optional
        Where to write ``infomap_nodes.parquet``, ``communities.parquet``,
        and the run summary ``infomap_run.json``. If ``None``, nothing is
        written and only the in-memory result is returned.
    args : str, optional
        Raw Infomap CLI arguments passed to :class:`~infomap.Infomap`.
    entities_name : str, optional
        Entity file name inside ``input_dir``. Default ``"entities.parquet"``.
    relationships_name : str, optional
        Relationship file name inside ``input_dir``. Default
        ``"relationships.parquet"``.
    entity_id_col, entity_title_col, source_col, target_col : str, optional
        Column names passed to :func:`read_graphrag`.
    weight_col, relationship_id_col, endpoint_col : str, optional
        Column names passed to :func:`read_graphrag`; see there for details.
    silent : bool, optional
        Suppress Infomap console output. Default ``True``.
    seed : int, optional
        Random number generator seed. Default ``123``.
    num_trials : int, optional
        Number of independent trials; the best solution is kept. Default
        ``5``.
    **infomap_options
        Additional keyword arguments passed to :class:`~infomap.Infomap`.

    Returns
    -------
    GraphRAGRunResult
        The Infomap instance, the parsed graph, the output directory, and
        the nodes/communities tables.

    Raises
    ------
    TypeError
        If ``options=`` is passed; use ``args=`` for raw CLI arguments.
    ValueError
        If ``summary_json`` is passed; it is managed through ``output_dir``.
    """
    from infomap import Infomap

    if "options" in infomap_options:
        raise TypeError(
            "options= is not supported; use args=... for raw Infomap CLI arguments."
        )
    if "summary_json" in infomap_options:
        raise ValueError(
            "run_graphrag_communities manages summary_json through output_dir."
        )

    input_path = Path(input_dir)
    graph = read_graphrag(
        input_path / entities_name,
        input_path / relationships_name,
        entity_id_col=entity_id_col,
        entity_title_col=entity_title_col,
        source_col=source_col,
        target_col=target_col,
        weight_col=weight_col,
        relationship_id_col=relationship_id_col,
        endpoint_col=endpoint_col,
    )
    paths = _output_paths(output_dir)
    if paths["dir"] is not None:
        paths["dir"].mkdir(parents=True, exist_ok=True)
        infomap_options["summary_json"] = str(paths["run"])

    im = Infomap(
        args=args,
        silent=silent,
        seed=seed,
        num_trials=num_trials,
        **infomap_options,
    )
    im.add_links(_links_array(graph))
    result = im.run()
    nodes, communities = _build_tables(im, graph)
    if paths["dir"] is not None:
        nodes.to_parquet(paths["nodes"], index=False)
        communities.to_parquet(paths["communities"], index=False)
    return GraphRAGRunResult(
        infomap=im,
        result=result,
        graph=graph,
        output_dir=paths["dir"],
        nodes=nodes,
        communities=communities,
    )
