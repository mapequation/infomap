"""The legacy ``Infomap`` result/build/config mirror is deprecated in favour of
the ``Result`` returned by :meth:`Infomap.run` and the ``Network`` / functional
``infomap.run`` builders.

These members are deprecated by documentation only: each carries a
``.. deprecated::`` note in its docstring, but they emit **no**
``DeprecationWarning`` at runtime. Each deprecated member must therefore (a)
still work and return the same value as its non-deprecated replacement, and (b)
not raise under ``-W error::DeprecationWarning``. The package itself must also
not emit any of these warnings during normal operation.
"""

from __future__ import annotations

import warnings

import pytest

from infomap import Infomap, Options, run


def _two_triangles(**kwargs) -> Infomap:
    im = Infomap(silent=True, no_file_output=True, num_trials=2, **kwargs)
    for source, target in [(0, 1), (1, 2), (2, 0), (2, 3), (3, 4), (4, 5), (5, 3)]:
        im.add_link(source, target)
    return im


# -- no self-warning on normal operation ------------------------------------


@pytest.mark.fast
def test_no_self_warnings_on_import_construct_run_repr_summary():
    """Constructing, running, repr() and summary() emit zero DeprecationWarnings."""
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        im = _two_triangles()
        im.run()
        repr(im)
        im._repr_html_()
        im.summary()


# -- Result-accessor group ---------------------------------------------------


@pytest.mark.fast
def test_result_method_accessor_silent_and_matches():
    im = _two_triangles()
    result = im.run()
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        legacy = im.get_modules()
    assert legacy == result.modules()


@pytest.mark.fast
def test_result_property_accessor_silent_and_matches():
    im = _two_triangles()
    result = im.run()
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        legacy = list(im.flow_links)
    assert legacy == list(result.links(data="flow"))


@pytest.mark.fast
def test_result_metric_property_silent_and_matches():
    im = _two_triangles()
    result = im.run()
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        legacy = im.codelength
        legacy_levels = im.num_levels
    assert legacy == result.codelength
    assert legacy_levels == result.num_levels


@pytest.mark.fast
def test_deprecated_accessor_emits_no_warning():
    """Deprecated members are documentation-only: they emit no warning at all."""
    im = _two_triangles()
    im.run()
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im.get_modules()
    deprecations = [r for r in records if issubclass(r.category, DeprecationWarning)]
    assert deprecations == []


# -- build-from-graphs group -------------------------------------------------


@pytest.mark.fast
def test_build_from_graph_accessor_silent():
    scipy_sparse = pytest.importorskip("scipy.sparse")
    matrix = scipy_sparse.csr_matrix([[0, 1, 0], [1, 0, 1], [0, 1, 0]])
    im = Infomap(silent=True, no_file_output=True)
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        im.add_scipy_sparse_matrix(matrix)
    assert im.num_links > 0  # network was built and is not deprecated


@pytest.mark.fast
def test_from_graph_classmethod_silent():
    scipy_sparse = pytest.importorskip("scipy.sparse")
    matrix = scipy_sparse.csr_matrix([[0, 1, 0], [1, 0, 1], [0, 1, 0]])
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        im = Infomap.from_scipy_sparse_matrix(matrix, silent=True, no_file_output=True)
    assert im.num_links > 0


# -- config group ------------------------------------------------------------


@pytest.mark.fast
def test_run_with_options_silent_and_matches():
    from infomap import Options

    im = _two_triangles()
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        result = im.run_with_options(Options(num_trials=2))
    # Equivalent to running with the same options via the canonical path.
    assert result.codelength == im._result.codelength


@pytest.mark.fast
def test_from_options_silent():
    from infomap import Options

    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        im = Infomap.from_options(Options(num_trials=2), args=None)
    im.add_link(1, 2)
    assert im.run().num_top_modules >= 1


# -- docs-only policy --------------------------------------------------------


@pytest.mark.fast
def test_deprecated_member_still_documents_deprecation():
    """The docs-only policy: deprecated members keep their ``.. deprecated::`` note."""
    assert ".. deprecated::" in Infomap.codelength.__doc__


# -- advanced-tier keyword parameters (#741) --------------------------------


def _pending(records):
    return [
        str(r.message)
        for r in records
        if issubclass(r.category, PendingDeprecationWarning)
    ]


@pytest.mark.fast
def test_advanced_tier_kwargs_emit_no_deprecation_warning_and_work():
    """Advanced-tier kwargs never raise the louder DeprecationWarning (which
    tools escalate by default) and keep working."""
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        im = _two_triangles(core_loop_limit=5, flow_model="undirected")
        result = im.run(markov_time=1.0, core_loop_limit=5)
    assert result.num_top_modules >= 1


@pytest.mark.fast
def test_advanced_tier_kwarg_emits_pending_on_direct_call():
    """A non-default advanced-tier kwarg on a direct Infomap()/run() call
    emits a PendingDeprecationWarning naming the keyword (issue #741)."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        Infomap(silent=True, no_file_output=True, regularized=True)
    assert any("regularized" in message for message in _pending(records))

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im = _two_triangles()
        im.run(core_loop_limit=5)
    assert any("core_loop_limit" in message for message in _pending(records))


@pytest.mark.fast
def test_advanced_tier_message_follows_policy_action():
    """The message routes keep-tier kwargs to Options and args-only kwargs to
    the write_* methods -- never the contradictory 'via Options' for the
    latter (issue #755)."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        Infomap(silent=True, regularized=True, no_file_output=True)
    messages = _pending(records)
    regularized = next(m for m in messages if "regularized" in m)
    no_file_output = next(m for m in messages if "no_file_output" in m)
    assert "Options" in regularized
    assert "write_" in no_file_output and "Options" not in no_file_output


@pytest.mark.fast
def test_common_tier_kwargs_emit_no_pending_warning():
    """Common-tier kwargs stay on the signature and emit nothing."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im = Infomap(silent=True, num_trials=3, markov_time=2.0, two_level=True)
        im.add_link(0, 1)
        im.run(num_trials=2)
    assert _pending(records) == []


@pytest.mark.fast
def test_advanced_tier_kwarg_silent_through_options_and_adapters():
    """The Options path and the internal adapters funnel through the same
    Infomap()/run() but from inside the package, so they never warn -- using
    Options is the sanctioned, nag-free migration."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run(
            [(0, 1), (1, 2), (2, 0)],
            options=Options(regularized=True, core_loop_limit=5),
        )
    assert _pending(records) == []


@pytest.mark.fast
def test_functional_run_direct_advanced_kwarg_emits_pending():
    """A non-default advanced-tier kwarg typed directly on the functional
    infomap.run() front door emits a PendingDeprecationWarning naming the
    keyword. The front door funnels through Infomap(**resolved) from inside the
    package, so run() flags the user's own kwargs itself (issue #741)."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run([(0, 1), (1, 2), (2, 0)], regularized=True)
    assert any("regularized" in message for message in _pending(records))


@pytest.mark.fast
def test_functional_run_mapping_options_stays_silent():
    """A plain-mapping options carrier is blessed exactly like Options(...):
    advanced kwargs carried through it do not warn -- only bare keyword
    overrides do."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run([(0, 1), (1, 2), (2, 0)], options={"regularized": True})
    assert _pending(records) == []
