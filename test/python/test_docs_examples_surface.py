"""Our own examples and docs must not teach the deprecated API surface.

Enforces the steering decided on the 3.0 roadmap (#742): the runnable
examples and the documentation pages use the ``Result`` accessors, the
``Network.from_*`` builders, and carry advanced configuration through
``Options`` — never the docs-only-deprecated legacy mirror or advanced-tier
keyword arguments passed directly to ``Infomap(...)``/``run(...)``.

Receiver conventions checked (matching the docs' own naming): a deprecated
result/build accessor is a violation when read off an ``Infomap`` instance
named ``im``; the same attribute names on ``result``/``net`` are the blessed
surface. The API reference under ``source/api/`` is excluded — it documents
the deprecated members on purpose.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

import pytest


pytestmark = pytest.mark.fast

REPO_ROOT = Path(__file__).resolve().parents[2]
EXAMPLES = REPO_ROOT / "examples" / "python"
DOCS = REPO_ROOT / "interfaces" / "python" / "source"

# The legacy result/build/config mirror on Infomap (docs-only deprecated
# since 2.14; see test_deprecations.py). Curated by hand: introspecting
# docstrings would misclassify blessed members whose docstrings merely
# *contain* parameter-level deprecation directives (e.g. run()).
DEPRECATED_INSTANCE_MEMBERS = (
    "codelength",
    "codelengths",
    "effective_num_leaf_modules",
    "effective_num_top_modules",
    "elapsed_time",
    "entropy_rate",
    "flow_links",
    "get_dataframe",
    "get_effective_num_modules",
    "get_links",
    "get_modules",
    "get_multilevel_modules",
    "get_name",
    "get_names",
    "get_nodes",
    "get_state_names",
    "get_tree",
    "have_memory",
    "index_codelength",
    "leaf_modules",
    "links",
    "max_depth",
    "meta_codelength",
    "meta_entropy",
    "module_codelength",
    "modules",
    "multilevel_modules",
    "names",
    "nodes",
    "num_leaf_modules",
    "num_levels",
    "num_non_trivial_top_modules",
    "num_top_modules",
    "one_level_codelength",
    "physical_nodes",
    "physical_tree",
    "relative_codelength_savings",
    "state_names",
    "to_dataframe",
    "tree",
)

# Deprecated whichever receiver they are called on (only Infomap has them).
DEPRECATED_ANY_RECEIVER = (
    "get_dataframe",
    "get_effective_num_modules",
    "get_links",
    "get_modules",
    "get_multilevel_modules",
    "get_name",
    "get_names",
    "get_nodes",
    "get_state_names",
    "get_tree",
    "add_networkx_graph",
    "add_igraph_graph",
    "add_scipy_sparse_matrix",
    "add_edge_index",
    "run_with_options",
)

_IM_RECEIVER = re.compile(
    r"\bim\.(" + "|".join(DEPRECATED_INSTANCE_MEMBERS) + r")\b"
)
_ANY_RECEIVER = re.compile(
    r"\.(" + "|".join(DEPRECATED_ANY_RECEIVER) + r")\("
)
_DEPRECATED_CLASSMETHODS = re.compile(
    r"\bInfomap\.(from_options|from_scipy_sparse_matrix|from_edge_index)\("
)
# Infomap(...) construction, method run (any receiver), and the functional
# run(...) (bare name or module-qualified).
_CALL_HEADS = re.compile(r"(?:\bInfomap|\.run|(?<![\w.])run)\s*\(")


def _load_common_tier() -> set[str]:
    overrides = json.loads(
        (REPO_ROOT / "interfaces" / "parameters" / "overrides.json").read_text(
            encoding="utf-8"
        )
    )
    flags = overrides["tiers"]["python"]["common"]
    return {flag.removeprefix("--").replace("-", "_") for flag in flags}


def _advanced_kwargs() -> set[str]:
    from infomap._options import _OPTION_FIELD_NAMES

    return set(_OPTION_FIELD_NAMES) - _load_common_tier()


def _source_files() -> list[Path]:
    files = sorted(EXAMPLES.glob("*.py"))
    files += sorted(
        path
        for path in DOCS.rglob("*.md")
        if "api" not in path.relative_to(DOCS).parts
    )
    files += sorted(
        path
        for path in DOCS.rglob("*.rst")
        if "api" not in path.relative_to(DOCS).parts
    )
    return files


def _call_argument_spans(text: str) -> list[str]:
    """The argument text of every Infomap(...)/…run(...) call in ``text``."""
    spans = []
    for match in _CALL_HEADS.finditer(text):
        depth = 0
        start = match.end() - 1
        for position in range(start, len(text)):
            if text[position] == "(":
                depth += 1
            elif text[position] == ")":
                depth -= 1
                if depth == 0:
                    spans.append(text[start + 1 : position])
                    break
    return spans


_KWARG = re.compile(r"(?<![\w.\"'])(\w+)\s*=(?!=)")


def _strip_nested_option_calls(argument_text: str) -> str:
    # Kwargs inside Options(...) or options={...} are the blessed carrier;
    # remove those regions before scanning for direct advanced kwargs.
    argument_text = re.sub(r"Options\s*\([^)]*\)", "Options()", argument_text)
    argument_text = re.sub(r"options\s*=\s*\{[^}]*\}", "options={}", argument_text)
    return argument_text


# Deliberate exceptions. Keep this list short and justified; every entry
# should reference why the page must mention the deprecated surface.
ALLOWLIST: dict[str, set[str]] = {
    # The migration guide shows the legacy surface next to its replacement
    # on purpose -- that is the page's whole point.
    "interfaces/python/source/the-infomap-class.md": {
        "codelength",
        "get_modules",
        "nodes",
        "num_top_modules",
        "to_dataframe",
        "add_networkx_graph",
    },
}


def _allowed(rel: str, name: str) -> bool:
    return name in ALLOWLIST.get(rel, set())


def test_docs_and_examples_avoid_the_deprecated_surface():
    if not DOCS.is_dir():
        pytest.skip("docs source not available (wheel/sdist test run)")

    advanced = _advanced_kwargs()
    violations: list[str] = []

    for path in _source_files():
        text = path.read_text(encoding="utf-8", errors="ignore")
        rel = path.relative_to(REPO_ROOT)

        for match in _IM_RECEIVER.finditer(text):
            if not _allowed(str(rel), match.group(1)):
                violations.append(f"{rel}: im.{match.group(1)} (use the Result API)")
        for match in _ANY_RECEIVER.finditer(text):
            if not _allowed(str(rel), match.group(1)):
                violations.append(
                    f"{rel}: .{match.group(1)}() (deprecated; use Result/Network)"
                )
        for match in _DEPRECATED_CLASSMETHODS.finditer(text):
            if not _allowed(str(rel), match.group(1)):
                violations.append(
                    f"{rel}: Infomap.{match.group(1)}() (use Network.from_* or run())"
                )
        for span in _call_argument_spans(text):
            span = _strip_nested_option_calls(span)
            for kwarg in _KWARG.findall(span):
                if kwarg in advanced:
                    violations.append(
                        f"{rel}: advanced kwarg {kwarg}= passed directly "
                        "(carry it via Options)"
                    )

    assert not violations, (
        "our own examples/docs teach the deprecated or advanced-tier "
        "surface:\n" + "\n".join(sorted(set(violations)))
    )
