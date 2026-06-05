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


def test_read_graphrag_factorizes_titles_from_entities_first(tmp_path):
    from infomap.io.graphrag import read_graphrag

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

    from infomap.io.graphrag import read_graphrag

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

    from infomap.io.graphrag import read_graphrag

    graph = read_graphrag(entities_path, relationships_path, endpoint_col="id")

    assert graph.sources.tolist() == [1]
    assert graph.targets.tolist() == [2]


def test_write_graphrag_communities_exports_mvp_tables(tmp_path):
    pd = _require_parquet_stack()

    from infomap import Infomap
    from infomap.io.graphrag import read_graphrag, write_graphrag_communities

    entities_path, relationships_path = _write_graphrag_fixture(tmp_path)
    graph = read_graphrag(entities_path, relationships_path)

    im = Infomap(silent=True, seed=123, num_trials=5)
    im.add_links(graph.sources, graph.targets, graph.weights)
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
    assert run["num_nodes"] == 6
    assert run["num_links"] == 7
    assert run["codelength"] == pytest.approx(im.codelength)
    assert run["options"] == "--silent --num-trials 5"
    assert run["seed"] is None


def test_write_graphrag_communities_combines_constructor_and_run_options(tmp_path):
    from infomap import Infomap
    from infomap.io.graphrag import read_graphrag, write_graphrag_communities

    entities_path, relationships_path = _write_graphrag_fixture(tmp_path)
    graph = read_graphrag(entities_path, relationships_path)

    im = Infomap(silent=True, seed=7)
    im.add_links(graph.sources, graph.targets, graph.weights)
    im.run(num_trials=3)

    output_dir = tmp_path / "infomap"
    write_graphrag_communities(im, graph=graph, output=output_dir)

    run = json.loads((output_dir / "infomap_run.json").read_text())
    assert run["options"] == "--silent --seed 7 --num-trials 3"
    assert run["seed"] == 7


def test_run_graphrag_communities_reads_runs_and_writes_outputs(tmp_path):
    from infomap.io.graphrag import run_graphrag_communities

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
    assert run["options"] == "--silent --seed 123 --num-trials 1"
    assert run["seed"] == 123
