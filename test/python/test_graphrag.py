import json

import pytest


pytestmark = pytest.mark.fast


def _require_parquet_stack():
    pd = pytest.importorskip("pandas")
    pytest.importorskip("pyarrow")
    return pd


def _write_graphrag_fixture(tmp_path):
    pd = _require_parquet_stack()

    entities = pd.DataFrame(
        {
            "id": ["a", "b", "c", "d", "e", "f"],
            "title": ["Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta"],
        }
    )
    relationships = pd.DataFrame(
        {
            "id": ["ab", "bc", "ca", "de", "ef", "fd", "cd"],
            "source": ["Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta", "Gamma"],
            "target": ["Beta", "Gamma", "Alpha", "Epsilon", "Zeta", "Delta", "Delta"],
            "weight": [2.0, 2.0, 2.0, 3.0, 3.0, 3.0, 1.0],
        }
    )

    entities_path = tmp_path / "entities.parquet"
    relationships_path = tmp_path / "relationships.parquet"
    entities.to_parquet(entities_path)
    relationships.to_parquet(relationships_path)
    return entities_path, relationships_path


def _add_graphrag_links(im, graph):
    np = pytest.importorskip("numpy")
    columns = [graph.sources, graph.targets]
    if graph.weights is not None:
        columns.append(graph.weights)
    im.add_links(np.column_stack(columns))


def test_read_graphrag_factorizes_titles_from_entities_first(tmp_path):
    from infomap.graphrag import read_graphrag

    entities_path, relationships_path = _write_graphrag_fixture(tmp_path)

    graph = read_graphrag(entities_path, relationships_path)

    assert graph.entity_id_to_node_id == {
        "a": 1,
        "b": 2,
        "c": 3,
        "d": 4,
        "e": 5,
        "f": 6,
    }
    assert graph.node_id_to_entity_id[1] == "a"
    assert graph.node_id_to_entity_title[1] == "Alpha"
    assert graph.sources.tolist() == [1, 2, 3, 4, 5, 6, 3]
    assert graph.targets.tolist() == [2, 3, 1, 5, 6, 4, 4]
    assert graph.weights.tolist() == [2.0, 2.0, 2.0, 3.0, 3.0, 3.0, 1.0]


def test_read_graphrag_resolves_title_id_collisions_by_endpoint_columns(tmp_path):
    pd = _require_parquet_stack()

    entities = pd.DataFrame(
        {
            "id": ["id-alpha", "Alpha"],
            "title": ["Alpha", "Beta"],
        }
    )
    relationships = pd.DataFrame(
        {
            "source": ["Alpha"],
            "target": ["Beta"],
            "weight": [1.0],
        }
    )
    entities_path = tmp_path / "entities.parquet"
    relationships_path = tmp_path / "relationships.parquet"
    entities.to_parquet(entities_path)
    relationships.to_parquet(relationships_path)

    from infomap.graphrag import read_graphrag

    graph = read_graphrag(entities_path, relationships_path)

    assert graph.sources.tolist() == [1]
    assert graph.targets.tolist() == [2]


def test_read_graphrag_can_resolve_relationship_endpoints_by_entity_id(tmp_path):
    pd = _require_parquet_stack()

    entities = pd.DataFrame(
        {
            "id": ["id-alpha", "Alpha"],
            "title": ["Alpha", "Beta"],
        }
    )
    relationships = pd.DataFrame(
        {
            "source": ["id-alpha"],
            "target": ["Alpha"],
            "weight": [1.0],
        }
    )
    entities_path = tmp_path / "entities.parquet"
    relationships_path = tmp_path / "relationships.parquet"
    entities.to_parquet(entities_path)
    relationships.to_parquet(relationships_path)

    from infomap.graphrag import read_graphrag

    graph = read_graphrag(entities_path, relationships_path, endpoint_col="id")

    assert graph.sources.tolist() == [1]
    assert graph.targets.tolist() == [2]


def test_read_graphrag_rejects_missing_relationship_endpoints(tmp_path):
    pd = _require_parquet_stack()

    entities = pd.DataFrame(
        {
            "id": ["a", "b"],
            "title": ["Alpha", "Beta"],
        }
    )
    relationships = pd.DataFrame(
        {
            "source": ["Alpha", None],
            "target": ["Beta", "Alpha"],
            "weight": [1.0, 1.0],
        }
    )
    entities_path = tmp_path / "entities.parquet"
    relationships_path = tmp_path / "relationships.parquet"
    entities.to_parquet(entities_path)
    relationships.to_parquet(relationships_path)

    from infomap.graphrag import read_graphrag

    with pytest.raises(ValueError, match="missing relationship endpoints"):
        read_graphrag(entities_path, relationships_path)


def test_read_graphrag_rejects_duplicate_titles_for_title_endpoints(tmp_path):
    pd = _require_parquet_stack()

    entities = pd.DataFrame(
        {
            "id": ["a", "b"],
            "title": ["Alpha", "Alpha"],
        }
    )
    relationships = pd.DataFrame(
        {
            "source": ["Alpha"],
            "target": ["Alpha"],
            "weight": [1.0],
        }
    )
    entities_path = tmp_path / "entities.parquet"
    relationships_path = tmp_path / "relationships.parquet"
    entities.to_parquet(entities_path)
    relationships.to_parquet(relationships_path)

    from infomap.graphrag import read_graphrag

    with pytest.raises(ValueError, match="unique.*endpoint_col=.id."):
        read_graphrag(entities_path, relationships_path)


def test_output_paths_treats_dotted_output_as_directory(tmp_path):
    from infomap.graphrag import _output_paths

    output_dir = tmp_path / "output.v1"
    paths = _output_paths(output_dir)

    assert paths["dir"] == output_dir
    assert paths["communities"] == output_dir / "communities.parquet"

    output_file = tmp_path / "communities.v1.parquet"
    paths = _output_paths(output_file)

    assert paths["dir"] == tmp_path
    assert paths["communities"] == output_file


def test_write_graphrag_communities_exports_mvp_tables(tmp_path):
    pd = _require_parquet_stack()

    from infomap import Infomap
    from infomap.graphrag import read_graphrag, write_graphrag_communities

    entities_path, relationships_path = _write_graphrag_fixture(tmp_path)
    graph = read_graphrag(entities_path, relationships_path)

    im = Infomap(silent=True, seed=123, num_trials=5)
    _add_graphrag_links(im, graph)
    im.run()

    output_dir = tmp_path / "infomap"
    write_graphrag_communities(im, graph=graph, output=output_dir)

    nodes = pd.read_parquet(output_dir / "infomap_nodes.parquet")
    communities = pd.read_parquet(output_dir / "communities.parquet")
    run = json.loads((output_dir / "infomap_run.json").read_text())

    assert set(nodes.columns) == {
        "node_id",
        "entity_id",
        "entity_title",
        "module_id",
        "module_path",
        "level",
        "flow",
    }
    assert sorted(nodes["entity_id"].tolist()) == ["a", "b", "c", "d", "e", "f"]
    assert set(communities.columns) == {
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
    }
    assert communities["level"].tolist() == [1] * len(communities)
    community_relationships = {
        tuple(sorted(row["entity_ids"])): sorted(row["relationship_ids"])
        for _, row in communities.iterrows()
    }
    assert community_relationships[("a", "b", "c")] == ["ab", "bc", "ca"]
    assert community_relationships[("d", "e", "f")] == ["de", "ef", "fd"]
    assert run["codelength"] == pytest.approx(im.codelength)
    assert run["top_modules"] == im.num_top_modules
    assert run["levels"] == im.max_depth
    assert run["trials"] == 5
    assert "options" not in run
    assert "seed" not in run


def test_write_graphrag_communities_exports_hierarchy(tmp_path, example_network_path):
    pd = _require_parquet_stack()

    from infomap import Infomap
    from infomap.graphrag import read_graphrag, write_graphrag_communities

    edges = [
        tuple(line.split())
        for line in example_network_path("ninetriangles.net").read_text().splitlines()
    ]
    entities = pd.DataFrame(
        {
            "id": [str(node_id) for node_id in range(1, 28)],
            "title": [str(node_id) for node_id in range(1, 28)],
        }
    )
    relationships = pd.DataFrame(
        {
            "id": [f"r{i}" for i in range(len(edges))],
            "source": [source for source, _target in edges],
            "target": [target for _source, target in edges],
            "weight": [1.0] * len(edges),
        }
    )
    graph = read_graphrag(entities, relationships)

    im = Infomap(silent=True, seed=123, num_trials=5)
    _add_graphrag_links(im, graph)
    im.run()

    assert im.max_depth > 2

    output_dir = tmp_path / "infomap"
    write_graphrag_communities(im, graph=graph, output=output_dir)
    communities = pd.read_parquet(output_dir / "communities.parquet")

    assert communities["level"].max() > 1

    by_community = {int(row["community"]): row for _, row in communities.iterrows()}
    parent_ids = {int(parent) for parent in communities["parent"] if pd.notna(parent)}
    assert parent_ids <= set(by_community)

    top_level = communities[communities["level"] == 1]
    assert top_level["parent"].isna().all()

    for parent_id in parent_ids:
        child_ids = set(by_community[parent_id]["children"])
        actual_children = set(
            communities.loc[communities["parent"] == parent_id, "community"]
        )
        assert child_ids == actual_children

    leaf_paths = {tuple(path) for path in im.get_multilevel_modules().values()}
    expected_prefix_count = len(
        {path[:level] for path in leaf_paths for level in range(1, len(path) + 1)}
    )
    assert len(communities) == expected_prefix_count

    deepest_relationships = {
        tuple(sorted(row["entity_ids"])): sorted(row["relationship_ids"])
        for _, row in communities[
            communities["level"] == communities["level"].max()
        ].iterrows()
    }
    assert deepest_relationships[("1", "2", "3")] == ["r0", "r1", "r3"]


def test_write_graphrag_communities_does_not_depend_on_run_args_history(tmp_path):
    from infomap import Infomap
    from infomap.graphrag import read_graphrag, write_graphrag_communities

    entities_path, relationships_path = _write_graphrag_fixture(tmp_path)
    graph = read_graphrag(entities_path, relationships_path)

    im = Infomap(silent=True, seed=7)
    _add_graphrag_links(im, graph)
    im.run(num_trials=3)

    output_dir = tmp_path / "infomap"
    write_graphrag_communities(im, graph=graph, output=output_dir)

    run = json.loads((output_dir / "infomap_run.json").read_text())
    assert run["trials"] == 3
    assert run["codelength"] == pytest.approx(im.codelength)
    assert "options" not in run
    assert "seed" not in run


def test_run_graphrag_communities_reads_runs_and_writes_outputs(tmp_path):
    from infomap.graphrag import run_graphrag_communities

    entities_path, relationships_path = _write_graphrag_fixture(tmp_path)
    input_dir = entities_path.parent
    output_dir = tmp_path / "result"

    result = run_graphrag_communities(
        input_dir=input_dir,
        output_dir=output_dir,
        options="--silent --seed 123 --num-trials 1",
    )

    assert result.infomap.num_nodes == 6
    assert result.graph.relationships.shape[0] == 7
    assert (output_dir / "infomap_nodes.parquet").is_file()
    assert (output_dir / "infomap_run.json").is_file()
    assert (output_dir / "communities.parquet").is_file()

    run = json.loads((output_dir / "infomap_run.json").read_text())
    assert run["trials"] == 1
    assert run["codelength"] == pytest.approx(result.infomap.codelength)
    assert "options" not in run
    assert "seed" not in run
