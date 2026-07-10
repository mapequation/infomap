"""The graph finders accept an ``options=`` carrier (the 3.0-safe path).

``find_communities`` / ``find_igraph_communities`` historically took engine
options only as bare ``**infomap_options`` keyword arguments -- which forward to
``Infomap(...)`` and so leave no warning-free path once the advanced-tier
keywords move off that signature in 3.0. These tests pin the ``options=``
carrier added for that path: precedence (bare kwargs and ``trials`` win over the
carrier), the finder's ``num_trials`` default surviving a carrier left at its
own defaults, and the carrier never clobbering the finder's ``no_file_output`` /
``silent`` defaults.
"""

from __future__ import annotations

import networkx as nx
import pytest

import infomap
import infomap._facade as facade
from infomap import Options


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
        (Options(num_trials=7), {}, 7),          # carrier sets num_trials
        ({"num_trials": 5}, {}, 5),              # mapping carrier
        (Options(regularized=True), {}, 10),     # carrier at default -> finder default 10
        (Options(num_trials=1), {}, 10),         # explicit-but-default 1 -> treated as unset
        (Options(num_trials=7), {"num_trials": 3}, 3),  # bare kwarg wins
        (Options(num_trials=7), {"trials": 3}, 3),      # trials alias wins
    ],
)
def test_find_communities_options_carrier_num_trials(recorded, options, kwargs, expected):
    infomap.find_communities(_graph(), options=options, seed=1, **kwargs)
    assert recorded["num_trials"] == expected


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_find_communities_carrier_carries_advanced_option(recorded):
    infomap.find_communities(_graph(), options=Options(regularized=True))
    assert recorded["regularized"] is True


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_find_communities_carrier_does_not_disable_no_file_output(recorded):
    # The carrier's *default* no_file_output=False must not clobber the finder's
    # no_file_output=True (only fields the caller changed are folded in).
    infomap.find_communities(_graph(), options=Options(regularized=True))
    assert recorded["no_file_output"] is True
    assert recorded["silent"] is True


def test_find_communities_options_carrier_returns_labels():
    # End-to-end: the carrier path still produces a valid partition.
    communities = infomap.find_communities(
        _graph(), options=Options(num_trials=3, seed=123)
    )
    assert set().union(*communities) == {"a", "b", "c"}


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_find_igraph_communities_options_carrier_num_trials(recorded):
    ig = pytest.importorskip("igraph")
    g = ig.Graph.Formula("a-b, b-c")
    infomap.find_igraph_communities(g, options=Options(num_trials=7))
    assert recorded["num_trials"] == 7
    # A carrier at its defaults keeps the finder's num_trials default of 10.
    infomap.find_igraph_communities(g, options=Options(regularized=True))
    assert recorded["num_trials"] == 10
    assert recorded["regularized"] is True
    assert recorded["no_file_output"] is True
