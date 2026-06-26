"""Regression guard for the documented tree-walking iterator/node types.

These types are documented released API (``source/api/iterators.rst``) and are
the objects ``Infomap.tree`` / ``leaf_modules`` / the physical variants hand
back. A curated ``__all__`` once dropped them, silently breaking
``from infomap import InfoNode`` and ``isinstance(node, infomap.InfomapIterator)``
with no deprecation period. Pin them so the package surface can't regress again.
"""

import pytest

import infomap

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
    im = infomap.Infomap(silent=True, no_file_output=True)
    im.add_link(0, 1)
    im.add_link(1, 2)
    im.run()
    node = next(iter(im.tree))
    assert isinstance(node, infomap.InfomapIterator)
