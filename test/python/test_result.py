from __future__ import annotations

import pytest

from infomap import Infomap


def _two_triangles() -> Infomap:
    im = Infomap(silent=True, no_file_output=True)
    im.add_link(0, 1)
    im.add_link(1, 2)
    im.add_link(2, 0)
    im.add_link(2, 3)
    im.add_link(3, 4)
    im.add_link(4, 5)
    im.add_link(5, 3)
    return im


@pytest.mark.fast
def test_get_node_data_single_traversal_matches_get_modules():
    im = _two_triangles()
    im.run()
    nd = im._core.getNodeData(1, False)
    by_node = dict(zip(list(nd.node_id), list(nd.module_id)))
    assert by_node == im.get_modules(1, False)
