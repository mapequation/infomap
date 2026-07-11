"""The graph finders accept an ``options=`` carrier (the 3.0-safe path).

``find_communities`` / ``find_igraph_communities`` historically took engine
options only as bare ``**infomap_options`` keyword arguments -- which forward to
``Infomap(...)`` and so leave no warning-free path once the advanced-tier
keywords move off that signature in 3.0. These tests pin the ``options=``
carrier added for that path: precedence (bare kwargs and ``trials`` win over the
carrier), ``num_trials`` defaulting to 1 like :func:`infomap.run`, and the
finder staying quiet without forcing ``no_file_output``.
"""

from __future__ import annotations

import networkx as nx
import pytest

import infomap
import infomap._facade as facade


pytestmark = pytest.mark.fast


@pytest.fixture
def recorded(monkeypatch):
    """Capture the keyword options the finder hands to ``Infomap(...)``."""
    captured: dict = {}
    real_infomap = facade.Infomap

    class RecordingInfomap(real_infomap):
        def __init__(self, *args, **options):
            captured.clear()
            captured.update(options)
            super().__init__(*args, **options)

    monkeypatch.setattr(facade, "Infomap", RecordingInfomap)
    return captured


def _graph():
    return nx.Graph([("a", "b"), ("b", "c")])


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
@pytest.mark.parametrize(
    "options, kwargs, expected",
    [
        (infomap.Options(num_trials=7), {}, 7),          # carrier sets num_trials
        ({"num_trials": 5}, {}, 5),              # mapping carrier
        (infomap.Options(regularized=True), {}, 1),      # carrier without num_trials -> default 1
        (infomap.Options(num_trials=7), {"num_trials": 3}, 3),  # bare kwarg wins
        (infomap.Options(num_trials=7), {"trials": 3}, 3),      # trials alias wins
    ],
)
def test_find_communities_options_carrier_num_trials(recorded, options, kwargs, expected):
    infomap.find_communities(_graph(), options=options, seed=1, **kwargs)
    assert recorded.get("num_trials", 1) == expected


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_find_communities_carrier_carries_advanced_option(recorded):
    infomap.find_communities(_graph(), options=infomap.Options(regularized=True))
    assert recorded["regularized"] is True


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_find_communities_carrier_keeps_engine_quiet_without_forcing_no_file_output(recorded):
    # The finder no longer force-sets silent or no_file_output; the Options
    # carrier already defaults silent True and no_file_output False, so the
    # engine reaches run() quiet and file-free without the finder touching them.
    infomap.find_communities(_graph(), options=infomap.Options(regularized=True))
    assert recorded["silent"] is True
    assert recorded["no_file_output"] is False


def test_find_communities_options_carrier_returns_labels():
    # End-to-end: the carrier path still produces a valid partition.
    communities = infomap.find_communities(
        _graph(), options=infomap.Options(num_trials=3, seed=123)
    )
    assert set().union(*communities) == {"a", "b", "c"}


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_find_igraph_communities_options_carrier_num_trials(recorded):
    ig = pytest.importorskip("igraph")
    g = ig.Graph.Formula("a-b, b-c")
    infomap.find_igraph_communities(g, options=infomap.Options(num_trials=7))
    assert recorded.get("num_trials", 1) == 7
    # A carrier without num_trials uses the engine default (1), like run().
    infomap.find_igraph_communities(g, options=infomap.Options(regularized=True))
    assert recorded.get("num_trials", 1) == 1
    assert recorded["regularized"] is True
    assert recorded["silent"] is True
