"""Our own examples and docs must not teach the deprecated API surface.

Enforces the steering decided on the 3.0 roadmap (#742): the runnable
examples, the example notebooks (and their ``_support.py`` helper), and the
documentation pages use the ``Result`` accessors, the
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
SRC = REPO_ROOT / "interfaces" / "python" / "src" / "infomap"

# The legacy result/build/config mirror on Infomap (deprecated in 2.15;
# silent-by-default PendingDeprecationWarning; see test_deprecations.py).
# Curated by hand: introspecting
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

_IM_RECEIVER = re.compile(r"\bim\.(" + "|".join(DEPRECATED_INSTANCE_MEMBERS) + r")\b")
_ANY_RECEIVER = re.compile(r"\.(" + "|".join(DEPRECATED_ANY_RECEIVER) + r")\(")
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
    flags = [
        flag
        for flag, per_surface in overrides["policy"]["parameters"].items()
        if per_surface.get("python", {}).get("tier") == "common"
    ]
    assert flags, "no common-tier parameters found in the policy"
    return {flag.removeprefix("--").replace("-", "_") for flag in flags}


def _advanced_kwargs() -> set[str]:
    from infomap._options import _OPTION_FIELD_NAMES

    return set(_OPTION_FIELD_NAMES) - _load_common_tier()


def _rst_python_blocks(text: str) -> str:
    """The bodies of ``.. code-block:: python`` directives in an RST file.

    The repo-root README mixes Python, R, and shell snippets in one file, so a
    whole-file scan would flag the R example's ``silent = TRUE``. Restrict it to
    the Python directives. (Docs under ``source/`` are Python-only, so they are
    still scanned whole.)
    """
    lines = text.splitlines()
    blocks: list[str] = []
    index = 0
    directive = re.compile(r"^\s*\.\.\s+code(?:-block)?::\s*python\s*$")
    while index < len(lines):
        if not directive.match(lines[index]):
            index += 1
            continue
        index += 1
        body: list[str] = []
        # Skip blank lines between the directive and its indented body.
        while index < len(lines) and not lines[index].strip():
            index += 1
        for line in lines[index:]:
            if line.strip() and not line.startswith((" ", "\t")):
                break
            body.append(line)
            index += 1
        blocks.append("\n".join(body))
    return "\n".join(blocks)


def _notebook_code(text: str) -> str:
    """The concatenated source of every code cell in a Jupyter notebook.

    Markdown cells are prose; only code cells carry the runnable surface, so
    (as with the README's Python-only blocks) we scan code cells alone.
    """
    notebook = json.loads(text)
    sources: list[str] = []
    for cell in notebook.get("cells", []):
        if cell.get("cell_type") != "code":
            continue
        source = cell.get("source", "")
        sources.append("".join(source) if isinstance(source, list) else source)
    return "\n".join(sources)


def _scannable_text(path: Path) -> str:
    text = path.read_text(encoding="utf-8", errors="ignore")
    if path.name == "README.rst":
        return _rst_python_blocks(text)
    if path.suffix == ".ipynb":
        return _notebook_code(text)
    return text


def _source_files() -> list[Path]:
    files = sorted(EXAMPLES.glob("*.py"))
    # The repo-root README is the front door; keep its Python snippet on the
    # supported surface too (it lives outside source/, so it needs listing).
    readme = REPO_ROOT / "README.rst"
    if readme.is_file():
        files.append(readme)
    # The example notebooks (and their _support.py helper) are held to the same
    # supported surface as the .py examples. Only code cells are scanned (see
    # _notebook_code); markdown prose may still name the legacy surface.
    notebooks = REPO_ROOT / "examples" / "notebooks"
    if notebooks.is_dir():
        files += sorted(notebooks.glob("*.ipynb"))
        support = notebooks / "_support.py"
        if support.is_file():
            files.append(support)
    files += sorted(
        path for path in DOCS.rglob("*.md") if "api" not in path.relative_to(DOCS).parts
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
        "modules",
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
        text = _scannable_text(path)
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


# The package's own source modules are the help()/autodoc surface, so their
# docstrings (and code) must not pass an advanced-tier option directly either --
# otherwise an agent reading help(Infomap.set_meta_data) learns the 3.0-breaking
# bare-kwarg pattern. Only the advanced-kwarg rule applies here; the
# instance-accessor rules do NOT, because the deprecated members are *defined*
# and documented on their own in these files. The console-quieting
# silent/verbosity_level knobs are excluded: they default to silent, do not emit
# the pending-deprecation warning, and are covered by the separate
# silent->logging migration.
_CONSOLE_KWARGS = {"silent", "verbosity_level"}


def _source_module_files() -> list[Path]:
    # _swig.py is the low-level SWIG-generated binding, not an authored surface.
    return [p for p in sorted(SRC.rglob("*.py")) if p.name != "_swig.py"]


@pytest.mark.fast
def test_source_docstrings_avoid_direct_advanced_kwargs():
    if not SRC.is_dir():
        pytest.skip("package source not available (wheel/sdist test run)")

    advanced = _advanced_kwargs() - _CONSOLE_KWARGS
    violations: list[str] = []
    for path in _source_module_files():
        text = path.read_text(encoding="utf-8", errors="ignore")
        rel = path.relative_to(REPO_ROOT)
        for span in _call_argument_spans(text):
            span = _strip_nested_option_calls(span)
            for kwarg in _KWARG.findall(span):
                if kwarg in advanced:
                    violations.append(
                        f"{rel}: advanced kwarg {kwarg}= passed directly "
                        "(carry it via Options)"
                    )
    assert not violations, (
        "source docstrings/code pass advanced-tier kwargs directly:\n"
        + "\n".join(sorted(set(violations)))
    )
