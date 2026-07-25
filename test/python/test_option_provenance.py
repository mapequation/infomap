"""An explicit keyword must win over the options carrier, even at a default.

``Infomap()`` / ``Infomap.run()`` merge their per-keyword parameters with the
``options=`` carrier. The merge used to decide "did the caller pass this?" by
comparing the value against the default, so a keyword explicitly set to a value
that happens to equal the default was indistinguishable from silence and lost to
the carrier -- inverting the documented "overrides win over options" rule. The
default seed is 123, which makes ``seed=123`` precisely the value a user is most
likely to pass explicitly.

``infomap.run()`` and ``Network.run()`` were already correct, because they mark
their common-tier parameters with an unset sentinel rather than comparing values.
These tests pin all three surfaces to the same rule.
"""

from __future__ import annotations

import infomap
import pytest
from infomap import Options
from infomap._options import _merge_options

pytestmark = pytest.mark.fast

_LINKS = [(1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 6), (6, 4)]

# The values a caller is most likely to pass explicitly *and* which equal their
# own default, so the old value-comparison could not see them.
_DEFAULTS = Options()


def _rendered(options: Options) -> str:
    return options.to_args()


@pytest.mark.parametrize("context", ["init", "run"])
def test_explicit_override_equal_to_the_default_wins(context):
    merged = _merge_options(Options(seed=7), {"seed": _DEFAULTS.seed}, context)

    assert merged.seed == _DEFAULTS.seed
    # The carrier's value must not reach the engine. The default-valued seed
    # itself renders no flag, which is behaviour-preserving: the spec defaults
    # are generated from the engine's own parameter catalog, so an elided
    # default and an explicit one give the same run.
    assert "--seed 7" not in _rendered(merged)


@pytest.mark.parametrize("context", ["init", "run"])
def test_explicit_false_flag_overrides_a_true_carrier(context):
    merged = _merge_options(Options(two_level=True), {"two_level": False}, context)

    assert merged.two_level is False
    assert "--two-level" not in _rendered(merged)


@pytest.mark.parametrize("context", ["init", "run"])
def test_an_unpassed_keyword_still_defers_to_the_carrier(context):
    # The other half of the rule, and the one that must not regress: silence
    # means "use the carrier", so an empty override mapping changes nothing.
    merged = _merge_options(Options(seed=7, two_level=True), {}, context)

    assert merged.seed == 7
    assert merged.two_level is True


def test_the_override_reaches_the_engine(network_fixture_path):
    # End to end, on an effect that is visible in the result rather than on a
    # codelength that two seeds might coincidentally share: two_level=False is
    # the default, so the old merge dropped it and the carrier's True won,
    # collapsing a genuinely hierarchical network to two levels.
    fixture = str(network_fixture_path("unbalanced_hierarchy.net"))

    im = infomap.Infomap(options=Options(two_level=True), two_level=False, seed=1)
    im.read_file(fixture)
    explicit_false = im.run()

    carrier_only = infomap.Infomap(options=Options(two_level=True), seed=1)
    carrier_only.read_file(fixture)
    two_level = carrier_only.run()

    assert two_level.num_levels == 2
    assert explicit_false.num_levels > 2


def test_facade_and_functional_run_agree_on_the_same_call(network_fixture_path):
    # infomap.run() was already correct; the facade must now resolve the same
    # call the same way rather than differently.
    fixture = str(network_fixture_path("unbalanced_hierarchy.net"))

    im = infomap.Infomap(options=Options(two_level=True), two_level=False, seed=1)
    im.read_file(fixture)

    functional = infomap.run(
        infomap.Network.from_file(fixture),
        options=Options(two_level=True),
        two_level=False,
        seed=1,
    )

    assert im.run().num_levels == functional.num_levels


def test_the_common_tier_signature_marks_unset_parameters():
    # The mechanism, so a regeneration that drops the sentinel is caught here
    # rather than as a silently wrong merge somewhere downstream.
    import inspect

    from infomap._options import _UNSET

    for surface in (infomap.Infomap.__init__, infomap.Infomap.run):
        parameters = inspect.signature(surface).parameters
        for name in ("seed", "num_trials", "two_level", "directed", "markov_time"):
            assert parameters[name].default is _UNSET, (
                f"{surface.__qualname__}({name}=...) lost its unset sentinel"
            )
