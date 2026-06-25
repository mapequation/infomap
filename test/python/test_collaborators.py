"""Focused unit tests for the facade's collaborator decomposition (SP1)."""

import pytest

from infomap import Infomap


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
