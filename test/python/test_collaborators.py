"""Focused unit tests for the facade's collaborator decomposition (SP1)."""

import pytest
from infomap import Infomap, Network


@pytest.mark.fast
def test_network_builder_is_used_and_builds():
    im = Infomap(silent=True)
    assert isinstance(im._network, Network)
    im.add_node(1, "a")
    im.add_node(2)
    im.add_links([(1, 2), (2, 3)])
    im.run()
    assert im.num_nodes == 3
    assert im.num_links >= 2
    assert im.get_name(1) == "a"


@pytest.mark.fast
def test_repr_html_and_summary_render():
    im = Infomap(silent=True)
    im.add_links([(1, 2), (2, 3), (3, 1)])
    im.run()
    html = im._repr_html_()
    # Assert real rendered content, not just that some <div> exists.
    assert isinstance(html, str)
    assert "infomap-metric" in html
    assert "Codelength" in html  # primary metric label shown after a run
    assert "Top-module flow" in html  # flow strip rendered for a run network
    summary = im.summary()
    assert isinstance(summary, dict)
    assert summary["status"] == "run"
    assert summary["nodes"] == 3


@pytest.mark.fast
def test_multilayer_builder_is_used_and_builds():
    im = Infomap(silent=True)
    # The unified Network owns the multilayer verbs too (no separate builder).
    assert isinstance(im._network, Network)
    im.add_multilayer_intra_links([(1, 1, 2), (1, 2, 1), (2, 2, 3), (2, 3, 2)])
    im.add_multilayer_inter_links([(1, 2, 2), (2, 2, 1)])
    im.run()
    assert im.num_top_modules >= 1
    assert im.have_memory
