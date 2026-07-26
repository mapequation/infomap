"""The CI path filters must cover the files whose gates they trigger.

`.github/filters.yml` decides which lanes run on a pull request, and
`ci-summary` treats a skipped lane as success. A gate whose inputs match no
pattern in its own lane's filter therefore does not merely run less often --
it never runs, and the required check goes green with nothing verified.

Two gates own files outside the lane that hosts them, which is what these tests
pin:

- the Python SWIG freshness gate (`_python-package.yml`, keyed off the
  `python-swig` filter) regenerates the wrapper from the headers the SWIG
  interface `%include`s;
- the binding-option freshness gate (`make test-binding-options-freshness`,
  invoked only from `_js-package.yml`, keyed off the `javascript` filter) owns
  generated artifacts under `interfaces/python` and `interfaces/R`.

Both lists are derived here from their real sources rather than duplicated, so a
new `%include` or a new generated artifact fails this test instead of silently
widening the hole.
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest
import yaml

pytestmark = pytest.mark.fast

REPO_ROOT = Path(__file__).resolve().parents[2]
FILTERS = REPO_ROOT / ".github" / "filters.yml"
SWIG_INTERFACE_DIR = REPO_ROOT / "interfaces" / "swig"
GENERATOR = REPO_ROOT / "scripts" / "generate_binding_options.py"


def _flatten(value):
    """filters.yml uses YAML anchors, so a filter is a list of str and lists."""
    for item in value:
        if isinstance(item, list):
            yield from _flatten(item)
        else:
            yield item


def _pattern_to_regex(pattern: str) -> re.Pattern[str]:
    """Translate the glob subset dorny/paths-filter uses into a regex.

    Handles `**` (crossing separators) and `*` (within one segment), which is
    everything filters.yml actually uses.
    """
    out: list[str] = []
    index = 0
    while index < len(pattern):
        if pattern.startswith("**/", index):
            out.append("(?:.*/)?")
            index += 3
        elif pattern.startswith("**", index):
            out.append(".*")
            index += 2
        elif pattern[index] == "*":
            out.append("[^/]*")
            index += 1
        else:
            out.append(re.escape(pattern[index]))
            index += 1
    return re.compile(f"^{''.join(out)}$")


@pytest.fixture(scope="module")
def filters() -> dict[str, list[str]]:
    if not FILTERS.is_file():
        pytest.skip("filters.yml not available in this layout")
    loaded = yaml.safe_load(FILTERS.read_text())
    return {
        name: list(_flatten(value))
        for name, value in loaded.items()
        if isinstance(value, list) and not name.startswith("_")
    }


def _uncovered(paths: list[str], patterns: list[str]) -> list[str]:
    compiled = [_pattern_to_regex(p) for p in patterns]
    return [path for path in paths if not any(r.match(path) for r in compiled)]


def _swig_included_headers() -> list[str]:
    if not SWIG_INTERFACE_DIR.is_dir():
        pytest.skip("SWIG interface files not available in this layout")
    include = re.compile(r'^\s*%include\s+"(src/[^"]+)"', re.MULTILINE)
    found = {
        match.group(1)
        for path in sorted(SWIG_INTERFACE_DIR.glob("*.i"))
        for match in include.finditer(path.read_text())
    }
    assert found, "no %include of a src/ header found; the parser or layout changed"
    return sorted(found)


def _binding_option_artifacts() -> list[str]:
    """The tracked outputs of scripts/generate_binding_options.py.

    Read from the generator's own module-level path constants so a new output
    shows up here without anyone remembering to add it.
    """
    if not GENERATOR.is_file():
        pytest.skip("generator not available in this layout")
    text = GENERATOR.read_text()
    found = re.findall(r'^[A-Z_]+_OUT\s*=\s*Path\("([^"]+)"\)', text, re.MULTILINE)
    assert found, "no *_OUT = Path(...) constants found; the generator changed shape"
    return sorted(set(found))


def test_swig_wrapped_headers_trigger_the_swig_freshness_filter(filters):
    """A header SWIG wraps must fire `python-swig`, or the gate never runs.

    ci.yml derives `run-swig-freshness` from this filter, so a header edit that
    matches nothing here leaves the generated wrapper unchecked.
    """
    headers = _swig_included_headers()
    uncovered = _uncovered(headers, filters["python-swig"])
    assert not uncovered, (
        "these headers are wrapped by SWIG but match no python-swig pattern, so the "
        f"freshness gate is skipped when they change: {uncovered}"
    )


def test_binding_option_artifacts_trigger_the_lane_that_checks_them(filters):
    """Every generated binding-option artifact must fire the `javascript` lane.

    That lane is the only place `make test-binding-options-freshness` runs. An
    artifact that does not match it can be hand-edited and merged with the gate
    that owns it skipped.
    """
    artifacts = _binding_option_artifacts()
    uncovered = _uncovered(artifacts, filters["javascript"])
    assert not uncovered, (
        "these artifacts are checked by make test-binding-options-freshness, which "
        "only runs in the javascript lane, but match no javascript pattern: "
        f"{uncovered}"
    )


def test_roxygen_sources_trigger_the_lane_that_checks_the_man_pages(filters):
    """A roxygen comment change must fire the `r` filter.

    `make test-r-man-freshness` runs only in that lane, and the man pages had
    drifted from their sources with nothing to catch it. A roxygen comment lives in
    an ordinary R source file, so the coverage that matters is that those files and
    the generated man pages both match the filter -- otherwise a PR editing one
    skips the lane and the gate never runs.
    """
    r_files = sorted(
        str(path.relative_to(REPO_ROOT))
        for path in (REPO_ROOT / "interfaces" / "R" / "infomap").rglob("*")
        if path.suffix in {".R", ".Rd"} and path.is_file()
    )
    if not r_files:
        pytest.skip("R package not available in this layout")
    uncovered = _uncovered(r_files, filters["r"])
    assert not uncovered, (
        "these R sources or man pages match no `r` pattern, so a change to them "
        f"would skip the lane that runs make test-r-man-freshness: {uncovered}"
    )


def test_the_generator_is_itself_covered(filters):
    """The generator and its inputs must fire the lane that runs its gate."""
    inputs = [
        "scripts/generate_binding_options.py",
        "scripts/render_parameter_policy.py",
        "scripts/parameter_catalog.py",
        "interfaces/parameters/overrides.json",
    ]
    assert not _uncovered(inputs, filters["javascript"])
