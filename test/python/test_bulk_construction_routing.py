"""Guard: bulk link construction must keep routing into the optimized C++ bulk
constructors, never degrade into per-element Python loops. Protects the
performance invariant through the facade decomposition.
"""

import numpy as np
import pytest
from infomap import Infomap
from infomap._bindings import InfomapWrapper


def _owner(method_name):
    """The SWIG class that actually defines the bulk constructor; patching it
    intercepts the call regardless of the Core forwarding in between."""
    if method_name in InfomapWrapper.__dict__ or hasattr(InfomapWrapper, method_name):
        return InfomapWrapper
    raise AssertionError(f"{method_name} not found on InfomapWrapper")


@pytest.fixture
def spy(monkeypatch):
    calls = {"addLinks": 0, "addLinksFromNumpy2D": 0}
    for name in calls:
        owner = _owner(name)

        def make(counter_name):
            def stub(self, *args, **kwargs):
                calls[counter_name] += 1

            return stub

        monkeypatch.setattr(owner, name, make(name), raising=True)
    return calls


@pytest.mark.fast
def test_list_links_route_to_bulk_addLinks(spy):
    im = Infomap(silent=True)
    im.add_links([(1, 2), (1, 3)])
    assert spy == {"addLinks": 1, "addLinksFromNumpy2D": 0}


@pytest.mark.fast
def test_numpy_links_route_to_bulk_numpy(spy):
    im = Infomap(silent=True)
    im.add_links(np.array([[1, 2, 1.0], [2, 3, 2.0]]))
    assert spy == {"addLinks": 0, "addLinksFromNumpy2D": 1}


# method, list input, numpy input, bulk-list C++ name, bulk-numpy C++ name, per-element C++ name
_MULTILAYER_BULK = [
    (
        "add_multilayer_intra_links",
        [(1, 1, 2), (1, 2, 3)],
        np.array([[1, 1, 2, 1.0]]),
        "addMultilayerIntraLinks",
        "addMultilayerIntraLinksFromNumpy2D",
        "addMultilayerIntraLink",
    ),
    (
        "add_multilayer_inter_links",
        [(1, 1, 2), (2, 1, 1)],
        np.array([[1, 1, 2, 1.0]]),
        "addMultilayerInterLinks",
        "addMultilayerInterLinksFromNumpy2D",
        "addMultilayerInterLink",
    ),
    (
        "add_multilayer_links",
        [((0, 1), (1, 2))],
        np.array([[0, 1, 1, 2, 1.0]]),
        "addMultilayerLinks",
        "addMultilayerLinksFromNumpy2D",
        "addMultilayerLink",
    ),
]


@pytest.mark.fast
@pytest.mark.parametrize(
    "method,list_in,numpy_in,bulk,bulk_numpy,singular", _MULTILAYER_BULK
)
def test_multilayer_bulk_routing(
    monkeypatch, method, list_in, numpy_in, bulk, bulk_numpy, singular
):
    calls = {bulk: 0, bulk_numpy: 0, singular: 0}
    for name in calls:
        owner = _owner(name)

        def make(counter_name):
            def stub(self, *args, **kwargs):
                calls[counter_name] += 1

            return stub

        monkeypatch.setattr(owner, name, make(name), raising=True)

    getattr(Infomap(silent=True), method)(list_in)
    assert calls[bulk] == 1, "list input must route to the bulk C++ constructor"
    assert calls[singular] == 0, "list input must NOT loop per-element in Python"

    getattr(Infomap(silent=True), method)(numpy_in)
    assert calls[bulk_numpy] == 1, (
        "numpy input must route to the FromNumpy2D constructor"
    )
    assert calls[singular] == 0
