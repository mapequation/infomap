"""Guard: the transitional SWIG-forwarding ``__getattr__`` is gone.

Phase 5 removed ``Infomap.__getattr__`` (the forward into ``self._core``).
Undocumented SWIG names must no longer resolve on an ``Infomap`` instance,
while the documented public surface keeps working.
"""

import pytest

from infomap import Infomap


def test_infomap_defines_no_getattr_forward():
    assert "__getattr__" not in Infomap.__dict__


def test_undocumented_swig_name_raises_attribute_error():
    im = Infomap(silent=True)
    with pytest.raises(AttributeError):
        im.setDirected


def test_documented_surface_still_works():
    im = Infomap(silent=True)
    im.add_link(0, 1)
    im.run()
    assert im.codelength is not None
    assert im.network is not None
    assert im.num_nodes == 2
