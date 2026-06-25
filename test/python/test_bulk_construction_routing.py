"""Guard: bulk link construction must keep routing into the optimized C++ bulk
constructors, never degrade into per-element Python loops. Protects the
performance invariant through the facade decomposition.
"""

import numpy as np
import pytest

from infomap import Infomap


def _owner(method_name):
    """Class in Infomap's MRO that defines method_name, so patching it
    intercepts both ``super().m(...)`` and ``self._im.m(...)`` call styles."""
    for cls in Infomap.__mro__:
        if method_name in cls.__dict__:
            return cls
    raise AssertionError(f"{method_name} not found in Infomap MRO")


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
    im = Infomap(silent=True, no_file_output=True)
    im.add_links([(1, 2), (1, 3)])
    assert spy == {"addLinks": 1, "addLinksFromNumpy2D": 0}


@pytest.mark.fast
def test_numpy_links_route_to_bulk_numpy(spy):
    im = Infomap(silent=True, no_file_output=True)
    im.add_links(np.array([[1, 2, 1.0], [2, 3, 2.0]]))
    assert spy == {"addLinks": 0, "addLinksFromNumpy2D": 1}
