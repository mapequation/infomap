"""Options is the canonical public name for the frozen options dataclass.

InfomapOptions stays as a back-compat alias of the same class; the transient
Settings alias has been removed.
"""

import dataclasses
import warnings

import infomap
import pytest
from infomap import InfomapOptions, Options


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
def test_options_replace_returns_tweaked_copy_without_mutating():
    base = Options(num_trials=20, seed=123)
    tweaked = base.replace(num_trials=5)
    assert tweaked.num_trials == 5
    assert tweaked.seed == 123  # carried over
    assert base.num_trials == 20  # base unchanged
    assert tweaked is not base


@pytest.mark.fast
def test_options_replace_validates_like_construction():
    base = Options(num_trials=20)
    # Unknown field -> the same "did you mean" TypeError as Options(...).
    with pytest.raises(TypeError, match="did you mean 'num_trials'"):
        base.replace(num_trial=5)
    # Out-of-range value -> the same ValueError as Options(...).
    with pytest.raises(ValueError, match="out of range"):
        base.replace(seed=0)


@pytest.mark.fast
def test_options_repr_shows_only_non_default_fields():
    # A ~70-field frozen-dataclass repr is unreadable; the custom __repr__ shows
    # only the fields set away from their defaults and stays round-trippable.
    assert repr(Options()) == "Options()"
    assert repr(Options(num_trials=10)) == "Options(num_trials=10)"
    o = Options(num_trials=10, seed=1, flow_model="directed")
    assert eval(repr(o), {"Options": Options}) == o


@pytest.mark.fast
def test_directed_and_matching_flow_model_do_not_warn():
    # directed=True is shorthand for flow_model="directed" (False -> undirected),
    # so pairing them consistently is redundant but valid input -- to_args() must
    # not second-guess it. Only a genuine disagreement warns (see below).
    for directed, flow_model in ((True, "directed"), (False, "undirected")):
        with warnings.catch_warnings():
            warnings.simplefilter("error")  # any warning fails the test
            Options(directed=directed, flow_model=flow_model).to_args()


@pytest.mark.fast
def test_directed_conflicting_flow_model_warns():
    # A real disagreement: 'directed' silently wins and the flow_model is lost,
    # so the caller should hear about it.
    with pytest.warns(UserWarning, match="disagree"):
        Options(directed=True, flow_model="undirected").to_args()
    with pytest.warns(UserWarning, match="disagree"):
        Options(directed=False, flow_model="directed").to_args()
    # A directed variant is distinct from plain "directed" and is dropped too.
    with pytest.warns(UserWarning, match="disagree"):
        Options(directed=True, flow_model="undirdir").to_args()


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
