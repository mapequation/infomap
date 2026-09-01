"""The legacy ``Infomap`` result/build/config mirror is deprecated in favour of
the ``Result`` returned by :meth:`Infomap.run` and the ``Network`` / functional
``infomap.run`` builders.

The legacy result accessors emit a **silent-by-default**
``LEGACY_SURFACE_WARNING`` naming their ``Result`` replacement (the
caller-frame guard keeps internal readers -- the summary card, ``_repr_html_``,
in-package reuse -- quiet, so only user code is flagged). Each such member must
therefore (a) still work and return the same value as its replacement, (b) emit
that ``LEGACY_SURFACE_WARNING`` when read from user code, and (c) never raise
the louder ``DeprecationWarning`` that tools escalate by default. The package
itself must emit neither warning during normal operation (construct, run, repr,
summary).

The ``add_*`` build-from-graph adapters stay documentation-only for now (no
runtime warning); the result mirror and the legacy config / constructor helpers
(``from_options``, ``run_with_options``, ``from_scipy_sparse_matrix``,
``from_edge_index``) emit the pending warning.
"""

from __future__ import annotations

import warnings

import pytest
from infomap import Infomap, Options, run
from infomap._options import LEGACY_SURFACE_WARNING, TYPED_PARAMETER_WARNING


def _two_triangles(**kwargs) -> Infomap:
    # No silent= (advanced-tier, pending-deprecated); the API is quiet by
    # default, so the helper stays warning-free at construction.
    im = Infomap(num_trials=2, **kwargs)
    for source, target in [(0, 1), (1, 2), (2, 0), (2, 3), (3, 4), (4, 5), (5, 3)]:
        im.add_link(source, target)
    return im


# -- no self-warning on normal operation ------------------------------------


@pytest.mark.fast
def test_no_self_warnings_on_import_construct_run_repr_summary():
    """Construct, run, repr() and summary() emit neither a DeprecationWarning nor
    the accessors' LEGACY_SURFACE_WARNING. The summary card and _repr_html_
    read the legacy accessors internally, but the caller-frame guard keeps
    in-package reads quiet."""
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        warnings.simplefilter("error", LEGACY_SURFACE_WARNING)
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
        warnings.simplefilter("ignore", LEGACY_SURFACE_WARNING)
        legacy = im.get_modules()
    assert legacy == result.modules()


@pytest.mark.fast
def test_result_property_accessor_silent_and_matches():
    im = _two_triangles()
    result = im.run()
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        warnings.simplefilter("ignore", LEGACY_SURFACE_WARNING)
        legacy = list(im.flow_links)
    assert legacy == list(result.links(data="flow"))


@pytest.mark.fast
def test_result_metric_property_silent_and_matches():
    im = _two_triangles()
    result = im.run()
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        warnings.simplefilter("ignore", LEGACY_SURFACE_WARNING)
        legacy = im.codelength
        legacy_levels = im.num_levels
    assert legacy == result.codelength
    assert legacy_levels == result.num_levels


def _assert_legacy_tier_only(records, member):
    """The member warns on the legacy tier and not on the louder typed-parameter one.

    These three used to assert silence under ``simplefilter("error",
    DeprecationWarning)``, which stopped describing them when the legacy tier moved
    up to that class (#915). The distinction they exist to protect is the tier
    boundary, so that is what they assert now -- and they gained the positive half:
    the warning is actually emitted, which the old form never checked.
    """
    matching = [r for r in records if member in str(r.message)]
    assert matching, f"expected a deprecation warning naming {member}"
    assert all(issubclass(r.category, LEGACY_SURFACE_WARNING) for r in matching)
    assert not [
        r
        for r in matching
        if issubclass(r.category, TYPED_PARAMETER_WARNING)
        and not issubclass(r.category, LEGACY_SURFACE_WARNING)
    ]


@pytest.mark.fast
def test_deprecated_result_accessor_warns_on_the_legacy_tier_naming_its_replacement():
    """A legacy result accessor read from user code warns on the legacy tier and
    names its Result replacement, and never on the louder typed-parameter tier.

    The legacy tier is visible in ``__main__`` under PEP 565 rather than silent: it
    was ``PendingDeprecationWarning``, which CPython's default filters ignore, so the
    3.0 removal window reached nobody (#915).
    """
    im = _two_triangles()
    im.run()
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im.get_modules()
    legacy = [r for r in records if issubclass(r.category, LEGACY_SURFACE_WARNING)]
    assert any("result.modules()" in str(r.message) for r in legacy)
    # Reading a legacy accessor is not the same as typing a deprecated parameter,
    # and the two tiers stay distinct: only the latter warns unconditionally.
    assert not [
        r
        for r in records
        if issubclass(r.category, TYPED_PARAMETER_WARNING)
        and not issubclass(r.category, LEGACY_SURFACE_WARNING)
    ]


@pytest.mark.fast
def test_result_getattr_redirects_legacy_accessors():
    """A legacy Infomap accessor typed on a Result raises an AttributeError that
    names the Result replacement -- the method/property flip (``im.modules`` was
    a property, ``result.modules()`` is a method) trips agents porting old code.
    """
    im = _two_triangles()
    result = im.run()
    with pytest.raises(AttributeError, match=r"result\.modules\(\)"):
        result.get_modules
    with pytest.raises(AttributeError, match=r'result\.links\(data="flow"\)'):
        result.flow_links
    # An unknown attribute keeps the standard AttributeError, so dunder and
    # duck-typing probes still degrade normally.
    with pytest.raises(AttributeError, match="has no attribute"):
        result.definitely_not_a_real_attribute


# -- build-from-graphs group -------------------------------------------------


@pytest.mark.fast
def test_build_from_graph_accessor_silent():
    scipy_sparse = pytest.importorskip("scipy.sparse")
    matrix = scipy_sparse.csr_matrix([[0, 1, 0], [1, 0, 1], [0, 1, 0]])
    im = Infomap(silent=True)
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        im.add_scipy_sparse_matrix(matrix)
    assert im.num_links > 0  # network was built and is not deprecated


@pytest.mark.fast
def test_from_graph_classmethod_warns_only_on_the_legacy_tier():
    scipy_sparse = pytest.importorskip("scipy.sparse")
    matrix = scipy_sparse.csr_matrix([[0, 1, 0], [1, 0, 1], [0, 1, 0]])
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im = Infomap.from_scipy_sparse_matrix(matrix, silent=True)
    assert im.num_links > 0
    _assert_legacy_tier_only(records, "from_scipy_sparse_matrix")


# -- config group ------------------------------------------------------------


@pytest.mark.fast
def test_run_with_options_warns_on_the_legacy_tier_and_matches():
    from infomap import Options

    im = _two_triangles()
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        result = im.run_with_options(Options(num_trials=2))
    # Equivalent to running with the same options via the canonical path.
    assert result.codelength == im._result.codelength
    _assert_legacy_tier_only(records, "run_with_options")


@pytest.mark.fast
def test_from_options_warns_only_on_the_legacy_tier():
    from infomap import Options

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im = Infomap.from_options(Options(num_trials=2), args=None)
    im.add_link(1, 2)
    assert im.run().num_top_modules >= 1
    _assert_legacy_tier_only(records, "from_options")


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
        if issubclass(r.category, LEGACY_SURFACE_WARNING)
    ]


@pytest.mark.fast
def test_advanced_tier_kwargs_emit_no_deprecation_warning_and_work():
    """Advanced-tier kwargs never raise the louder DeprecationWarning (which
    tools escalate by default) and keep working."""
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        # Advanced-tier kwargs on the stateful surface do emit the softer
        # LEGACY_SURFACE_WARNING by design; this test is only about the
        # louder DeprecationWarning, so ignore the pending one here.
        warnings.simplefilter("ignore", LEGACY_SURFACE_WARNING)
        im = _two_triangles(core_loop_limit=5, flow_model="undirected")
        result = im.run(markov_time=1.0, core_loop_limit=5)
    assert result.num_top_modules >= 1


@pytest.mark.fast
def test_advanced_tier_kwarg_emits_pending_on_direct_call():
    """A non-default advanced-tier kwarg on a direct Infomap()/run() call
    emits a LEGACY_SURFACE_WARNING naming the keyword (issue #741)."""
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
def test_advanced_tier_lead_follows_policy_action():
    """A `remove` keyword leaves the Python surface, not just the signatures.

    The lead used to say "deprecated on the Infomap() and run() signatures and leaves
    them in 3.0" for every advanced-tier keyword. For a keyword classified ``remove``
    that named a refuge which does not exist -- its replacement is another option, the
    logging module or the CLI binary, never ``Options`` -- and it contradicted the
    ``.. deprecated:: Not a Python library option`` note on the same field in the
    Options reference (#915).
    """
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        Infomap(silent=True, threads=4, num_random_moves=7)
    messages = _pending(records)

    removed = next(m for m in messages if "'threads'" in m)
    assert "leaves the Python surface in 3.0" in removed
    # Naming the signatures would point at Options, where it does not survive.
    assert "signatures" not in removed

    # A keyword that really does only leave the signatures keeps the old lead, and
    # Options is still where it goes.
    kept = next(m for m in messages if "'num_random_moves'" in m)
    assert "signatures and leaves them in 3.0" in kept
    assert "Options" in kept


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
def test_functional_run_direct_advanced_kwarg_does_not_warn():
    """A bare advanced engine kwarg on the functional infomap.run() front door
    forwards to Options without a deprecation warning: run(**kwargs) is a
    permanent convenience, and only the giant explicit Infomap()/Infomap.run()
    signatures are slimmed in 3.0 (issue #741)."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run([(0, 1), (1, 2), (2, 0)], regularized=True)
    assert _pending(records) == []


@pytest.mark.fast
def test_functional_run_mapping_options_stays_silent():
    """A plain-mapping options carrier is blessed exactly like Options(...):
    advanced kwargs carried through it do not warn -- only bare keyword
    overrides do."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run([(0, 1), (1, 2), (2, 0)], options={"regularized": True})
    assert _pending(records) == []


@pytest.mark.fast
def test_legacy_result_accessor_replacements_resolve_on_result():
    """Every deprecated instance accessor names a ``Result`` replacement that
    actually exists -- a stale or typo'd "use X instead" hint would otherwise
    ship silently and mislead an agent following the steering."""
    import re

    from infomap._results import _LEGACY_RESULT_ACCESSORS

    result = run([(0, 1), (1, 2), (2, 0), (2, 3), (3, 4), (4, 5), (5, 3)], seed=1)
    for member, replacement in _LEGACY_RESULT_ACCESSORS.items():
        match = re.match(r"result\.([A-Za-z_][A-Za-z0-9_]*)", replacement)
        assert match, f"{member!r} hint {replacement!r} is not a result.* form"
        attr = match.group(1)
        assert hasattr(result, attr), (
            f"{member!r} points at result.{attr}, absent on Result"
        )


@pytest.mark.fast
def test_deprecated_infomap_methods_emit_pending():
    """The legacy ``from_options`` / ``run_with_options`` methods are documented
    ``.. deprecated::`` and now emit a LEGACY_SURFACE_WARNING naming their
    replacement, aligning with the legacy result accessors (they previously
    warned nowhere)."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        Infomap.from_options(Options(num_trials=1))
    assert any("from_options" in message for message in _pending(records))

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        _two_triangles().run_with_options(Options(num_trials=1))
    assert any("run_with_options" in message for message in _pending(records))


@pytest.mark.fast
def test_deprecated_infomap_graph_constructors_emit_pending():
    """``from_scipy_sparse_matrix`` / ``from_edge_index`` emit the same
    LEGACY_SURFACE_WARNING as the other deprecated ``Infomap`` methods."""
    sp = pytest.importorskip("scipy.sparse")
    np = pytest.importorskip("numpy")

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        Infomap.from_scipy_sparse_matrix(sp.csr_matrix((3, 3)))
    assert any("from_scipy_sparse_matrix" in message for message in _pending(records))

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        Infomap.from_edge_index(np.array([[0, 1], [1, 0]]))
    assert any("from_edge_index" in message for message in _pending(records))
