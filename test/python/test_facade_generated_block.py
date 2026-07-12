"""Guard: the generated Infomap.__init__/run signature block stays present and
in sync with the parameter catalog. The byte-level freshness is enforced by
``scripts/generate_binding_options.py --check`` (wired into
``make test-binding-options-freshness``); this test guards the runtime
invariants.
"""

import inspect
from pathlib import Path

import pytest

from infomap import Infomap
from infomap._options import _OPTION_FIELD_NAMES

FACADE = Path(__file__).resolve().parents[2] / (
    "interfaces/python/src/infomap/_facade.py"
)
BEGIN = "# === BEGIN generated: Infomap option signatures"
END = "# === END generated ==="

NON_OPTION_INIT = {"self", "args", "pretty", "options"}
NON_OPTION_RUN = {"self", "args", "initial_partition", "pretty", "options"}


@pytest.mark.fast
def test_marker_block_present():
    text = FACADE.read_text(encoding="utf-8")
    assert BEGIN in text
    assert END in text
    assert text.index(BEGIN) < text.index(END)


@pytest.mark.fast
def test_init_options_match_catalog():
    params = set(inspect.signature(Infomap.__init__).parameters) - NON_OPTION_INIT
    assert params == set(_OPTION_FIELD_NAMES)


@pytest.mark.fast
def test_run_options_match_catalog():
    params = set(inspect.signature(Infomap.run).parameters) - NON_OPTION_RUN
    assert params == set(_OPTION_FIELD_NAMES)


def test_generated_signatures_are_annotated():
    import inspect

    from infomap import Infomap, Result
    from infomap._options import FlowModel, OutputFormat  # noqa: F401  (generated aliases)

    init_signature = inspect.signature(Infomap.__init__)
    run_signature = inspect.signature(Infomap.run)

    # Every generated parameter carries an annotation (args/self included).
    for signature in (init_signature, run_signature):
        unannotated = [
            name
            for name, parameter in signature.parameters.items()
            if name != "self" and parameter.annotation is inspect.Parameter.empty
        ]
        assert unannotated == []

    assert init_signature.return_annotation is None
    assert run_signature.return_annotation in (Result, "Result")

    # Choice parameters are typed through the generated Literal aliases.
    assert "Literal" in str(run_signature.parameters["flow_model"].annotation)
    assert "Literal" in str(run_signature.parameters["output"].annotation)
    assert run_signature.parameters["num_trials"].annotation is int


def test_docstring_deprecation_follows_the_signature_tier():
    from infomap import Infomap

    # The full per-option reference lives once, on Infomap.__init__; Infomap.run
    # documents only its run-specific params and points back here rather than
    # duplicating the ~500-line listing on both signatures.
    doc = Infomap.__init__.__doc__
    assert doc is not None
    # Both advanced-tier directives are present: kept keywords relocate to
    # Options (versionchanged), removed/args-only keywords leave the API
    # (deprecated). See _advanced_tier_directive in the generator.
    assert ".. versionchanged:: 2.15" in doc
    assert ".. deprecated:: 2.15" in doc
    # Common-tier parameters (#738) carry neither directive: none may appear
    # between a common param's entry and the next parameter entry.
    for common, following in [
        ("seed : ", "num_trials : "),
        ("num_trials : ", "core_loop_limit : "),
        ("two_level : ", "flow_model : "),
        ("directed : ", "recorded_teleportation : "),
        ("markov_time : ", "variable_markov_time : "),
    ]:
        start = doc.index(common)
        end = doc.index(following, start)
        segment = doc[start:end]
        assert ".. deprecated::" not in segment, common
        assert ".. versionchanged::" not in segment, common
    # A kept advanced param relocates to Options: versionchanged, NOT
    # deprecated (marking a permanent tuning knob deprecated misleads).
    start = doc.index("regularized : ")
    segment = doc[start : doc.index("regularization_strength : ", start)]
    assert ".. versionchanged:: 2.15" in segment
    assert ".. deprecated::" not in segment
    # A removed keyword truly leaves the API: deprecated.
    start = doc.index("silent : ")
    assert ".. deprecated:: 2.15" in doc[start : doc.index("pretty : ", start)]
    # Output-artifact params are args-only: deprecated + the inertness note.
    start = doc.index("output : ")
    end = doc.index("hide_bipartite_nodes : ", start)
    assert "Has no effect in the Python API" in doc[start:end]

    # Infomap.run does NOT duplicate that reference: it points back to Infomap /
    # Options for the per-option docs and carries none of the per-param
    # directives itself.
    assert Infomap.run.__doc__ is not None
    run_doc = " ".join(Infomap.run.__doc__.split())  # collapse wrap for phrase checks
    assert ":class:`Infomap`" in run_doc
    assert "full parameter reference" in run_doc
    assert "regularized :" not in run_doc
    assert ".. deprecated::" not in run_doc
    assert ".. versionchanged::" not in run_doc
