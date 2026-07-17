"""Regression guard for the documented tree-walking iterator/node types.

These types are documented released API (``source/api/iterators.rst``) and are
the objects ``Infomap.tree`` / ``leaf_modules`` / the physical variants hand
back. A curated ``__all__`` once dropped them, silently breaking
``from infomap import InfoNode`` and ``isinstance(node, infomap.InfomapIterator)``
with no deprecation period. Pin them so the package surface can't regress again.
"""

import infomap
import pytest

# Each is ``.. autoclass``-ed in source/api/iterators.rst.
DOCUMENTED_TYPES = [
    "InfoNode",
    "InfomapIterator",
    "InfomapIteratorPhysical",
    "InfomapLeafIterator",
    "InfomapLeafIteratorPhysical",
    "InfomapLeafModuleIterator",
]


@pytest.mark.fast
def test_documented_iterator_types_are_importable():
    missing = [name for name in DOCUMENTED_TYPES if not hasattr(infomap, name)]
    assert not missing, f"documented types missing from infomap namespace: {missing}"


@pytest.mark.fast
def test_documented_iterator_types_in_dunder_all():
    missing = [name for name in DOCUMENTED_TYPES if name not in infomap.__all__]
    assert not missing, f"documented types missing from infomap.__all__: {missing}"


@pytest.mark.fast
def test_tree_returns_a_documented_iterator_type():
    im = infomap.Infomap(silent=True)
    im.add_link(0, 1)
    im.add_link(1, 2)
    im.run()
    node = next(iter(im.tree))
    assert isinstance(node, infomap.InfomapIterator)


@pytest.mark.fast
def test_treenode_is_importable_from_the_package():
    # TreeNode is the return type of Result.nodes(); it must be importable from
    # the top level (for isinstance / type hints), like Result itself.
    assert hasattr(infomap, "TreeNode")
    assert "TreeNode" in infomap.__all__
    im = infomap.Infomap(silent=True)
    im.add_link(0, 1)
    assert isinstance(next(im.run().nodes()), infomap.TreeNode)


@pytest.mark.fast
def test_tl_namespace_exposes_curated_names_only():
    # infomap.tl is the Scanpy-style tools namespace; its internal typing
    # imports (Any/Mapping/...) must not be part of its public surface, and
    # the GraphRAG tools are re-exported here as first-class members.
    import infomap.tl as tl

    assert tl.__all__ == [
        "GraphRAGGraph",
        "GraphRAGRunResult",
        "infomap",
        "read_graphrag",
        "run_graphrag_communities",
        "write_graphrag_communities",
    ]
    for name in tl.__all__:
        assert hasattr(tl, name)


@pytest.mark.fast
def test_io_namespace_is_public():
    assert "io" in infomap.__all__
    assert infomap.io.export.__all__ == sorted(infomap.io.export.__all__)
    from infomap.io.export import to_networkx  # noqa: F401


@pytest.mark.fast
def test_io_namespace_reexports_export_helpers():
    # The result-export helpers are re-exported at the io namespace level
    # (e.g. infomap.io.to_networkx), mirroring infomap.tl, so io has a curated
    # public surface rather than only submodules.
    import infomap.io as io

    assert io.__all__ == list(infomap.io.export.__all__)
    for name in io.__all__:
        assert getattr(io, name) is getattr(infomap.io.export, name)
