"""Focused unit tests for the facade's collaborator decomposition (SP1)."""

import pytest

from infomap import Infomap
from infomap._network_builder import _NetworkBuilder


@pytest.mark.fast
def test_network_builder_is_used_and_builds():
    im = Infomap(silent=True, no_file_output=True)
    assert isinstance(im._network, _NetworkBuilder)
    im.add_node(1, "a")
    im.add_node(2)
    im.add_links([(1, 2), (2, 3)])
    im.run()
    assert im.num_nodes == 3
    assert im.num_links >= 2
    assert im.get_name(1) == "a"


@pytest.mark.fast
def test_repr_html_and_summary_render():
    im = Infomap(silent=True, no_file_output=True)
    im.add_links([(1, 2), (2, 3), (3, 1)])
    im.run()
    html = im._repr_html_()
    assert isinstance(html, str) and "<div" in html
    summary = im.summary()
    assert isinstance(summary, dict)
    assert summary["status"] == "run"
    assert summary["nodes"] == 3
