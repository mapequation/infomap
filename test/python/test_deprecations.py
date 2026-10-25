"""The legacy ``Infomap`` result/build/config mirror is deprecated in favour of
the ``Result`` returned by :meth:`Infomap.run` and the ``Network`` / functional
``infomap.run`` builders.

The legacy result accessors emit a ``LEGACY_SURFACE_WARNING`` naming their
``Result`` replacement (the caller-frame guard keeps internal readers -- the
summary card, ``_repr_html_``, in-package reuse -- quiet, so only user code is
flagged). It is a ``DeprecationWarning`` subclass: visible in ``__main__``
under PEP 565, where the ``python analysis.py`` audience that has to migrate
will see it, rather than the ``PendingDeprecationWarning`` CPython's default
filters ignored outright (#915). Each such member must therefore (a) still work
and return the same value as its replacement, (b) emit that
``LEGACY_SURFACE_WARNING`` when read from user code, and (c) never emit the
louder ``TYPED_PARAMETER_WARNING`` tier, which is reserved for a deprecated
parameter the caller actually typed. The package itself must emit neither
warning during normal operation (construct, run, repr, summary).

The ``add_*`` build-from-graph adapters stay documentation-only for now (no
runtime warning); the result mirror and the legacy config / constructor helpers
(``from_options``, ``run_with_options``, ``from_scipy_sparse_matrix``,
``from_edge_index``) emit the legacy-tier warning.
"""

from __future__ import annotations

import os
import subprocess
import sys
import textwrap
import warnings

import pytest
from infomap import Infomap, Options, run
from infomap._options import LEGACY_SURFACE_WARNING, TYPED_PARAMETER_WARNING


def _two_triangles(**kwargs) -> Infomap:
    # No silent= (advanced-tier, deprecated); the API is quiet by
    # default, so the helper stays warning-free at construction.
    im = Infomap(num_trials=2, **kwargs)
    for source, target in [(0, 1), (1, 2), (2, 0), (2, 3), (3, 4), (4, 5), (5, 3)]:
        im.add_link(source, target)
    return im


def _run_script(tmp_path, body: str, *args: str) -> subprocess.CompletedProcess:
    """Run ``body`` as its own script, with the warning filters a user would have.

    A subprocess, and a file rather than ``-c``, because the property under test is
    PEP 565's: CPython's default filters show a ``DeprecationWarning`` only when it
    is attributed to ``__main__``. In-process tests all install their own filters,
    so none of them can observe it. ``PYTHONWARNINGS`` and ``-W`` are kept out of
    the environment so the defaults are genuinely in force.
    """
    script = tmp_path / "analysis.py"
    script.write_text(textwrap.dedent(body), encoding="utf-8")
    env = {k: v for k, v in os.environ.items() if k != "PYTHONWARNINGS"}
    return subprocess.run(
        [sys.executable, str(script), *args],
        capture_output=True,
        text=True,
        check=False,
        env=env,
    )


@pytest.mark.fast
def test_legacy_warning_reaches_a_plain_script_run(tmp_path):
    """A legacy accessor read from ``python analysis.py`` prints on stderr.

    This is the contract the whole change exists for, and the one no in-process
    test can check: every other test here forces its own filter, so it would pass
    just as happily while CPython's defaults swallowed the warning -- which is
    exactly what ``PendingDeprecationWarning`` did before #915, and what a
    misattributed ``stacklevel`` (pointing inside the package rather than at
    ``__main__``) would silently reintroduce.
    """
    result = _run_script(
        tmp_path,
        """
        from infomap import Infomap

        im = Infomap(num_trials=1, seed=1)
        for source, target in [(0, 1), (1, 2), (2, 0)]:
            im.add_link(source, target)
        im.run()
        im.get_modules()
        """,
    )

    assert result.returncode == 0, result.stderr
    # CPython prints the concrete class, so this is also the evidence that a
    # DeprecationWarning *subclass* inherits PEP 565's __main__ visibility --
    # the property the tier's design depends on.
    assert "LegacySurfaceWarning" in result.stderr, result.stderr
    assert "Infomap.get_modules is deprecated" in result.stderr, result.stderr
    # Attributed to the user's script, not to a frame inside the package.
    assert "analysis.py" in result.stderr, result.stderr


@pytest.mark.fast
def test_package_is_quiet_in_a_plain_script_run(tmp_path):
    """The mirror: normal operation prints no warning under the same defaults.

    Without this, the assertion above could be satisfied by a package that warns
    indiscriminately.
    """
    result = _run_script(
        tmp_path,
        """
        from infomap import Infomap, Options, run

        im = Infomap(num_trials=1, seed=1)
        for source, target in [(0, 1), (1, 2), (2, 0)]:
            im.add_link(source, target)
        result = im.run()
        result.codelength
        result.modules()
        repr(im)
        im.summary()
        run([(0, 1), (1, 2)], options=Options(num_trials=1, seed=1))
        """,
    )

    assert result.returncode == 0, result.stderr
    assert "Warning" not in result.stderr, result.stderr


# -- no self-warning on normal operation ------------------------------------


@pytest.mark.fast
def test_no_self_warnings_on_import_construct_run_repr_summary():
    """Construct, run, repr() and summary() emit nothing from either tier.

    Both base classes are escalated, not just ``DeprecationWarning``: the legacy
    tier is a subclass of that one, but the typed-parameter tier derives from
    ``FutureWarning``, so a stray warning from that tier during normal operation
    passed this contract while the docstring claimed otherwise. The summary card
    and ``_repr_html_`` read the legacy accessors internally, and the
    caller-frame guard is what keeps those in-package reads quiet.
    """
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        warnings.simplefilter("error", FutureWarning)
        warnings.simplefilter("error", LEGACY_SURFACE_WARNING)
        warnings.simplefilter("error", TYPED_PARAMETER_WARNING)
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
def test_advanced_tier_kwargs_stay_on_the_legacy_tier_and_work():
    """Advanced-tier kwargs warn on the legacy tier only -- never the louder
    typed-parameter tier, and never a bare deprecation from anywhere else -- and
    they keep working.

    The filters are ordered: ``simplefilter`` prepends, so the legacy-tier ignore
    installed last is consulted first, and the two error filters still fire for
    everything else. That ordering only means something because the tier is its
    own subclass -- while ``LEGACY_SURFACE_WARNING`` was ``DeprecationWarning``
    itself, ignoring it swallowed every warning the error filter was there to
    catch, and this test asserted nothing (#915).
    """
    with warnings.catch_warnings():
        warnings.simplefilter("error", FutureWarning)
        warnings.simplefilter("error", DeprecationWarning)
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


# -- removed Options fields ---------------------------------------------------
#
# The four fields the 3.0 policy classifies `remove` for Python (threads,
# silent, verbosity_level, print_config_fingerprint) leave Options too: their
# replacement is another option, the logging module or the CLI binary. Until
# #915 they warned only as bare Infomap()/run() keywords -- the very route the
# advanced-tier message steers callers *away* from -- and never on Options
# itself, so a caller who followed the steering met the 3.0 TypeError with no
# notice at all.


def _removed_fields():
    from infomap._options import _REMOVED_FIELDS

    return _REMOVED_FIELDS


@pytest.mark.fast
def test_removed_fields_are_the_four_the_policy_names():
    assert set(_removed_fields()) == {
        "threads",
        "silent",
        "verbosity_level",
        "print_config_fingerprint",
    }


@pytest.mark.fast
@pytest.mark.parametrize(
    "field, value",
    [
        ("threads", 4),
        ("silent", False),
        ("verbosity_level", 2),
        ("print_config_fingerprint", True),
    ],
)
def test_removed_field_on_options_warns_once_with_the_surface_lead(field, value):
    """Options(<removed>=...) emits exactly one legacy-tier warning, with the same
    lead as the signature route and the field's catalog replacement, and it is
    attributed to the caller's line -- not to the ``<string>`` frame of the
    dataclass-synthesized ``__init__``, where PEP 565 would hide it."""
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        Options(**{field: value})
    messages = _pending(records)
    assert messages == [m for m in messages if f"'{field}'" in m]
    assert len(messages) == 1, messages
    assert messages[0].startswith(f"'{field}' leaves the Python surface in 3.0. ")
    assert _removed_fields()[field].replacement in messages[0]
    assert "signatures" not in messages[0]
    legacy = [r for r in records if issubclass(r.category, LEGACY_SURFACE_WARNING)]
    assert legacy[0].filename == __file__


@pytest.mark.fast
def test_removed_field_left_at_its_default_does_not_warn():
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        Options()
        Options(silent=True, verbosity_level=1, print_config_fingerprint=False)
        Options(regularized=True, core_loop_limit=5, num_threads=2)
    assert _pending(records) == []


@pytest.mark.fast
def test_removed_field_warns_once_across_the_internal_merges():
    """One announcement per typed field, however many Options the package builds
    on the way to the engine (the run-context base, the keyword merge, the
    rendered-args funnel, the inferred-flow-model fold)."""
    # Bare keyword on the constructor: the signature route announces it, and
    # the merge that follows must not announce it again.
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im = Infomap(threads=1, num_trials=1, seed=1)
        im.add_link(0, 1)
        im.run()
    assert len([m for m in _pending(records) if "'threads'" in m]) == 1

    # A mapping carrier on the constructor.
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im = Infomap(options={"threads": 1, "num_trials": 1, "seed": 1})
        im.add_link(0, 1)
        im.run()
    assert len([m for m in _pending(records) if "'threads'" in m]) == 1

    # The functional front door, mapping and bare keyword alike.
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run([(0, 1), (1, 2), (2, 0)], options={"threads": 1}, seed=1)
    assert len([m for m in _pending(records) if "'threads'" in m]) == 1

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run([(0, 1), (1, 2), (2, 0)], threads=1, seed=1)
    assert len([m for m in _pending(records) if "'threads'" in m]) == 1

    # An Options instance announced itself where it was built; passing it on
    # must not repeat the announcement.
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        options = Options(threads=1, seed=1)
    assert len([m for m in _pending(records) if "'threads'" in m]) == 1
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run([(0, 1), (1, 2), (2, 0)], options=options)
        run([(0, 1), (1, 2), (2, 0)], options=options, num_trials=2)
    assert _pending(records) == []


@pytest.mark.fast
def test_removed_field_on_network_run_warns_for_mapping_and_bare_keyword():
    from infomap import Network

    net = Network().add_links([(0, 1), (1, 2), (2, 0)])
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        net.run(options={"threads": 1, "seed": 1})
    assert len([m for m in _pending(records) if "'threads'" in m]) == 1

    # Network.run() has no signature route: a bare keyword used to slip through
    # the merge unannounced.
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        net.run(threads=1, seed=1)
    assert len([m for m in _pending(records) if "'threads'" in m]) == 1


@pytest.mark.fast
def test_removed_field_next_to_an_options_instance_warns_once():
    """run(edges, options=Options(...), threads=1): the instance announced itself
    already, the bare keyword has not, and the merge must announce exactly it."""
    options = Options(seed=1)
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run([(0, 1), (1, 2), (2, 0)], options=options, threads=1)
    assert [m for m in _pending(records) if "'threads'" in m] == _pending(records)
    assert len(_pending(records)) == 1


@pytest.mark.fast
def test_removed_field_through_an_in_package_adapter_warns_at_the_caller():
    """find_communities(g, threads=1) reaches Infomap(**kwargs) from inside the
    package, where the signature route's frame gate is silent by design; the
    option merge announces the removed field instead, attributed to the caller."""
    nx = pytest.importorskip("networkx")
    from infomap import find_communities

    graph = nx.Graph([(0, 1), (1, 2), (2, 0)])
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        find_communities(graph, threads=1, seed=1)
    legacy = [r for r in records if issubclass(r.category, LEGACY_SURFACE_WARNING)]
    assert [str(r.message) for r in legacy if "'threads'" in str(r.message)]
    assert len(legacy) == 1
    assert legacy[0].filename == __file__


@pytest.mark.fast
def test_options_carrier_through_the_adapters_is_not_re_announced():
    """find_communities(g, options=Options(threads=1)) and the legacy
    from_options / run_with_options used to flatten the carrier into keywords,
    so every field reached the merge as if freshly typed and the removed field
    was announced a second time. The carrier now travels as itself."""
    nx = pytest.importorskip("networkx")
    from infomap import find_communities

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        options = Options(threads=1, seed=1)
    assert len(_pending(records)) == 1

    graph = nx.Graph([(0, 1), (1, 2), (2, 0)])
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        find_communities(graph, options=options)
        # A bare keyword next to the carrier is still announced, once.
        find_communities(graph, options=options, print_config_fingerprint=True)
    assert _pending(records) == [
        m for m in _pending(records) if "'print_config_fingerprint'" in m
    ]
    assert len(_pending(records)) == 1

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im = Infomap.from_options(options)
        im.add_link(0, 1)
        im.run_with_options(options)
    # Only the method deprecations themselves, nothing about the carrier's fields.
    assert [m for m in _pending(records) if "'threads'" in m] == []
    assert all(
        "from_options" in m or "run_with_options" in m for m in _pending(records)
    )


@pytest.mark.fast
def test_removed_field_on_an_empty_graph_finder_is_still_announced():
    """The finders return before building an engine for an empty graph, so the
    constructor's merge never sees the keyword; the notice must not depend on
    the input's size."""
    nx = pytest.importorskip("networkx")
    from infomap import find_communities

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        assert find_communities(nx.Graph(), threads=4) == []
    assert len([m for m in _pending(records) if "'threads'" in m]) == 1
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        assert find_communities(nx.Graph(), options={"threads": 4}) == []
    assert len([m for m in _pending(records) if "'threads'" in m]) == 1
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        assert find_communities(nx.Graph(), seed=1) == []
    assert _pending(records) == []


@pytest.mark.fast
def test_removed_field_on_an_empty_igraph_finder_is_still_announced():
    ig = pytest.importorskip("igraph")
    from infomap import find_igraph_communities

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        find_igraph_communities(ig.Graph(), threads=4)
        find_igraph_communities(ig.Graph(), options={"threads": 4})
    assert len([m for m in _pending(records) if "'threads'" in m]) == 2


@pytest.mark.fast
def test_options_carrier_through_the_igraph_finder_is_not_re_announced():
    ig = pytest.importorskip("igraph")
    from infomap import find_igraph_communities

    with warnings.catch_warnings(record=True):
        warnings.simplefilter("always")
        options = Options(threads=1, seed=1)
    graph = ig.Graph(edges=[(0, 1), (1, 2), (2, 0)])
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        find_igraph_communities(graph, options=options)
    assert _pending(records) == []


@pytest.mark.fast
def test_run_context_default_decides_what_counts_as_typed():
    """On an instance, run(silent=True) is a real choice (the run context's no-op
    default is False) and is announced; run(silent=False) restates that default
    and is not. Mirrors the signature route this replaces for removed fields."""
    im = _two_triangles()
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im.run(silent=True)
    assert len([m for m in _pending(records) if "'silent'" in m]) == 1
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im.run(silent=False)
    assert _pending(records) == []


@pytest.mark.fast
def test_functional_run_on_an_instance_uses_the_run_context():
    """run(im, silent=True) agrees with im.run(silent=True): on an existing
    Infomap or Network a bare keyword is a run-context override, where the
    no-op default of a flag is False, so True is the typed choice and False is
    not. A mapping carrier stays a full Options, announced against the
    dataclass defaults, as on im.run(options=mapping)."""
    from infomap import Network

    im = _two_triangles()
    net = Network().add_links([(0, 1), (1, 2), (2, 0)])
    for instance in (im, net):
        with warnings.catch_warnings(record=True) as records:
            warnings.simplefilter("always")
            run(instance, silent=True, seed=1)
        assert len([m for m in _pending(records) if "'silent'" in m]) == 1
        with warnings.catch_warnings(record=True) as records:
            warnings.simplefilter("always")
            run(instance, silent=False, seed=1)
        assert _pending(records) == []
        with warnings.catch_warnings(record=True) as records:
            warnings.simplefilter("always")
            run(instance, options={"silent": False, "seed": 1})
        assert len([m for m in _pending(records) if "'silent'" in m]) == 1

    # And the direct route it mirrors, for the record.
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im.run(silent=False)
        im.run(options={"seed": 1})
    assert _pending(records) == []


@pytest.mark.fast
def test_pretty_in_a_mapping_carrier_still_reaches_the_constructor():
    """options={"pretty": True} on infomap.run() is not an engine option and must
    keep its own path to Infomap(), where it emits the typed-parameter warning."""
    with pytest.warns(TYPED_PARAMETER_WARNING, match="pretty is deprecated"):
        run([(0, 1), (1, 2), (2, 0)], options={"pretty": True, "seed": 1})


@pytest.mark.fast
def test_removed_field_on_options_is_visible_with_python_dash_c():
    """python -c compiles the caller's code as a "<string>" frame too, and that
    one must not be mistaken for the dataclass-synthesized __init__: the warning
    names the caller's own line 2 rather than overshooting the stack."""
    env = {k: v for k, v in os.environ.items() if k != "PYTHONWARNINGS"}
    result = subprocess.run(
        [sys.executable, "-c", "from infomap import Options\nOptions(threads=4)"],
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    assert "LegacySurfaceWarning" in result.stderr, result.stderr
    assert "<string>:2" in result.stderr, result.stderr


@pytest.mark.fast
def test_removed_field_on_options_is_visible_in_a_plain_script(tmp_path):
    """The point of the tier: under default filters, in __main__, a removed field
    on Options prints -- and names the user's own line, which the ``<string>``
    frame of the synthesized __init__ used to swallow."""
    result = _run_script(
        tmp_path,
        """
        from infomap import Options

        Options(threads=4)
        """,
    )
    assert result.returncode == 0, result.stderr
    assert "LegacySurfaceWarning" in result.stderr, result.stderr
    assert "'threads' leaves the Python surface in 3.0" in result.stderr, result.stderr
    # Line 4 of the dedented script is the Options(...) call.
    assert "analysis.py:4" in result.stderr, result.stderr
