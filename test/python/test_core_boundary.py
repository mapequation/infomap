"""Guard: the SWIG-compiled engine is reached only through infomap._core.Core,
and Infomap no longer inherits the SWIG wrapper."""

import pytest

from infomap import Infomap
from infomap._bindings import InfomapWrapper
from infomap._core import Core


@pytest.mark.fast
def test_core_wraps_a_single_swig_instance():
    core = Core("--silent --no-file-output")
    assert isinstance(core._im, InfomapWrapper)


@pytest.mark.fast
def test_core_forwards_unknown_attributes_to_swig():
    core = Core("--silent --no-file-output")
    # addLink/codelength are SWIG methods; they must be reachable through Core.
    core.addLink(0, 1)
    core.run("")
    assert core.codelength() >= 0.0


@pytest.mark.fast
def test_core_does_not_recurse_before_init():
    # __getattr__ must not blow up looking for _im before it is set.
    with pytest.raises(AttributeError):
        Core.__new__(Core).addLink
