"""Options is the canonical public name for the frozen options dataclass.

InfomapOptions stays as a back-compat alias of the same class; the transient
Settings alias has been removed.
"""

import dataclasses

import pytest

import infomap
from infomap import Options, InfomapOptions


@pytest.mark.fast
def test_infomap_options_is_options():
    assert InfomapOptions is Options


@pytest.mark.fast
def test_options_is_frozen():
    assert dataclasses.fields(Options)  # is a dataclass
    o = Options(num_trials=5)
    with pytest.raises(dataclasses.FrozenInstanceError):
        o.num_trials = 10


@pytest.mark.fast
def test_options_exported_in_package_all():
    assert "Options" in infomap.__all__
    assert "InfomapOptions" in infomap.__all__


@pytest.mark.fast
def test_settings_alias_removed():
    assert not hasattr(infomap, "Settings")
    assert "Settings" not in infomap.__all__


@pytest.mark.fast
def test_num_threads_annotation_accepts_str_and_int():
    # The engine option is string-typed ('auto' or a number), but the Python
    # surface also accepts an int (rendered into the CLI args). The annotation
    # is widened to str | int | None via the parameter-catalog `types` override
    # so a type checker agrees with the documented "or a positive integer".
    assert "int" in Options.__annotations__["num_threads"]
    assert Options(num_threads=1).num_threads == 1
    assert Options(num_threads="auto").num_threads == "auto"


@pytest.mark.fast
def test_options_roundtrips_through_facade():
    o = Options(num_trials=3, two_level=True)
    im = infomap.Infomap.from_options(o, args=None)
    im.add_link(1, 2)
    result = im.run_with_options(o)
    assert isinstance(result.codelength, float)
    assert result.num_top_modules >= 1
    modules = result.modules()
    assert set(modules) == {1, 2}  # both nodes partitioned
    assert len(set(modules.values())) == 1  # one link -> one shared module
