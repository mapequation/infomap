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

NON_OPTION_INIT = {"self", "args", "pretty"}
NON_OPTION_RUN = {"self", "args", "initial_partition", "pretty"}


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

    for doc in (Infomap.__init__.__doc__, Infomap.run.__doc__):
        assert doc is not None
        # Advanced-tier parameters carry the docs-only deprecation directive.
        assert ".. deprecated:: 2.15" in doc
        # Common-tier parameters (#738) do not: no directive may appear
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
            assert ".. deprecated::" not in doc[start:end], common
        # Output-artifact params carry the inertness note from #748.
        start = doc.index("output : ")
        end = doc.index("hide_bipartite_nodes : ", start)
        assert "Has no effect in the Python API" in doc[start:end]
