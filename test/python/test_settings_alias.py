"""Settings is the canonical public name for the frozen options dataclass.

InfomapOptions stays as a back-compat alias of the same class.
"""

import dataclasses

import pytest

import infomap
from infomap import Settings, InfomapOptions


@pytest.mark.fast
def test_settings_is_infomap_options():
    assert Settings is InfomapOptions


@pytest.mark.fast
def test_settings_is_frozen():
    assert dataclasses.fields(Settings)  # is a dataclass
    s = Settings(num_trials=5)
    with pytest.raises(dataclasses.FrozenInstanceError):
        s.num_trials = 10


@pytest.mark.fast
def test_settings_exported_in_package_all():
    assert "Settings" in infomap.__all__


@pytest.mark.fast
def test_settings_roundtrips_through_facade():
    s = Settings(num_trials=3, two_level=True)
    im = infomap.Infomap.from_options(s, args=None)
    im.add_link(1, 2)
    result = im.run_with_options(s)
    assert result is not None
