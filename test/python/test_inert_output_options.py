"""Output-artifact options set on the library surface warn instead of no-op.

The args-only options (``tree``, ``clu``, ``out_name``, ``output`` ...) write
files only when an output directory is supplied through the raw ``args`` escape
hatch. Set via :class:`Options` on the normal library surface they used to
construct and run without error but silently do nothing. These tests pin the
``UserWarning`` that now points such calls at the ``Result`` / ``Network``
writers, the raw-args escape staying quiet, and the ``no_file_output``
suppressor (used defensively by the graph finders) never tripping it.
"""

from __future__ import annotations

import warnings

import pytest
from infomap import Infomap, Network, Options, find_communities, run
from infomap._run import _warn_inert_output_options

pytestmark = pytest.mark.fast

LINKS = [(0, 1), (1, 2), (2, 0)]


def _inert_userwarnings(records):
    return [
        record
        for record in records
        if issubclass(record.category, UserWarning)
        and "output options" in str(record.message)
    ]


def test_helper_warns_without_the_raw_args_escape():
    with pytest.warns(UserWarning, match=r"tree.*write_tree"):
        _warn_inert_output_options(Options(tree=True), None)


def test_helper_stays_quiet_with_the_raw_args_escape():
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        _warn_inert_output_options(Options(tree=True), "some_output_dir")


def test_helper_warns_on_no_file_output_too():
    # no_file_output is inert on the library surface like the other output
    # flags (file output is controlled by the writers), so it warns as well.
    with pytest.warns(UserWarning, match="no_file_output"):
        _warn_inert_output_options(Options(no_file_output=True), None)


def test_run_options_carrier_output_flag_warns():
    with pytest.warns(UserWarning, match="write_tree"):
        run(LINKS, options=Options(tree=True))


def test_infomap_construction_options_carrier_warns():
    with pytest.warns(UserWarning, match="clu"):
        Infomap(options=Options(clu=True))


def test_network_run_options_carrier_warns():
    net = Network()
    net.add_link(0, 1)
    net.add_link(1, 2)
    net.add_link(2, 0)
    with pytest.warns(UserWarning, match="out_name"):
        net.run(options=Options(out_name="x"))


def test_hide_bipartite_nodes_is_not_treated_as_inert():
    # hide_bipartite_nodes is classified args-only, but unlike the other output
    # flags it is NOT inert on the library surface: it projects the secondary
    # (type-B) node type out of what the Result writers emit (write_tree /
    # write_clu). Warning "no effect, write from the Result instead" would be
    # wrong -- the writers are exactly where it takes effect -- so it must not
    # trip the inert-output warning.
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        _warn_inert_output_options(Options(hide_bipartite_nodes=True), None)
    assert _inert_userwarnings(records) == []


def test_hide_bipartite_nodes_excluded_but_genuine_inert_flag_still_warns():
    # When hide_bipartite_nodes rides alongside a genuinely inert flag, the
    # warning fires for the inert flag only and never names hide_bipartite_nodes.
    with pytest.warns(UserWarning, match="no_file_output") as records:
        _warn_inert_output_options(
            Options(hide_bipartite_nodes=True, no_file_output=True), None
        )
    assert not any(
        "hide_bipartite_nodes" in str(record.message) for record in records
    )


def test_writer_effective_set_matches_catalog_inertness():
    # The hand-maintained _WRITER_EFFECTIVE_ARGS_ONLY in _run.py must equal the
    # args-only Python options that are NOT flagged inertWithoutOutputDir in the
    # parameter catalog (overrides.json). The generated docstrings read the
    # inertWithoutOutputDir bit ("has no effect ... unless an output directory")
    # while the runtime warning reads this set; if the two drift, an option is
    # documented as inert while the writers honour it (the hide_bipartite_nodes
    # bug) or vice versa. Tie them together so a policy edit can't silently
    # reintroduce the mismatch.
    import json
    import sys
    from pathlib import Path

    from infomap._options import _ADVANCED_TIER_KWARGS
    from infomap._run import _WRITER_EFFECTIVE_ARGS_ONLY

    repo_root = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(repo_root / "scripts"))
    from parameter_catalog import resolve_policy_decision, snake_name

    overrides = json.loads(
        (repo_root / "interfaces" / "parameters" / "overrides.json").read_text(
            encoding="utf-8"
        )
    )
    args_only_flags = {
        "--" + name.replace("_", "-")
        for name, spec in _ADVANCED_TIER_KWARGS.items()
        if spec[2] == "args-only"
    }
    expected = {
        snake_name(flag)
        for flag in args_only_flags
        if not resolve_policy_decision(overrides, flag, "python").get(
            "inertWithoutOutputDir"
        )
    }
    assert _WRITER_EFFECTIVE_ARGS_ONLY == expected


def test_plain_run_does_not_warn():
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        run(LINKS)
    assert _inert_userwarnings(records) == []


def test_find_communities_emits_no_inert_output_warning():
    # The finders no longer force any output flag (no_file_output included), so
    # a plain call trips nothing.
    nx = pytest.importorskip("networkx")
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        find_communities(nx.Graph([("a", "b"), ("b", "c")]))
    assert _inert_userwarnings(records) == []
