"""Each open bug a fuzz target swallows is pinned by a C++ test that fails while it is open.

``isKnownFinding()`` in ``test/fuzz/fuzz_cluster.cpp`` and ``fuzz_run.cpp``
swallows the failures of open bugs, and no fuzz run notices when such a bug is
fixed. So each entry names its issue as ``#N``, and the C++ tests pin the bug as
a ``should_fail`` case with ``(#N)`` in its name. Fixing the bug fails that case;
dropping its marker then fails this test until the entry is removed too (#1261).
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest

pytestmark = pytest.mark.fast

REPO = Path(__file__).resolve().parents[2]

# Fuzz target -> the C++ test file that pins its known findings.
PINS = {
    "test/fuzz/fuzz_cluster.cpp": "test/cpp/test_partition.cpp",
    "test/fuzz/fuzz_run.cpp": "test/cpp/test_map_equation_invariants.cpp",
}

ISSUE = re.compile(r"#(\d+)")
# A string literal may hold escaped quotes.
TEST_CASE = re.compile(r'TEST_CASE\(\s*"((?:[^"\\\n]|\\.)*)"([^\n]*)')


def _read(relative: str) -> str:
    path = REPO / relative
    if not path.is_file():
        pytest.skip(f"{relative} not available in this layout")
    return path.read_text()


def known_issues(source: str) -> set[str]:
    """The issues named in the body of isKnownFinding()."""
    start = source.index("bool isKnownFinding(")
    open_brace = source.index("{", start)
    depth = 0
    for end in range(open_brace, len(source)):
        depth += {"{": 1, "}": -1}.get(source[end], 0)
        if depth == 0:
            return set(ISSUE.findall(source[open_brace:end]))
    raise AssertionError("isKnownFinding() has no closing brace")


def without_comments(source: str) -> str:
    """The C++ source with its comments blanked out and its string literals kept."""
    out = []
    i, n = 0, len(source)
    while i < n:
        if source[i] in "\"'":
            end = i + 1
            while end < n and source[end] not in source[i] + "\n":
                end += 2 if source[end] == "\\" else 1
            out.append(source[i : end + 1])
            i = end + 1
        elif source.startswith("//", i):
            end = source.find("\n", i)
            i = n if end < 0 else end
        elif source.startswith("/*", i):
            end = source.find("*/", i + 2)
            i = n if end < 0 else end + 2
            out.append(" ")
        else:
            out.append(source[i])
            i += 1
    return "".join(out)


def pinned_issues(source: str) -> set[str]:
    """The issues named by the TEST_CASEs marked should_fail, outside comments."""
    return {
        issue
        for name, rest in TEST_CASE.findall(without_comments(source))
        if "should_fail" in rest
        for issue in ISSUE.findall(name)
    }


@pytest.mark.parametrize(("target", "tests"), PINS.items(), ids=lambda p: Path(p).stem)
def test_known_finding_is_pinned_by_a_should_fail_case(target, tests):
    unpinned = known_issues(_read(target)) - pinned_issues(_read(tests))
    assert not unpinned, (
        f"isKnownFinding() in {target} names "
        f"{', '.join('#' + i for i in sorted(unpinned))} with no should_fail pin in "
        f"{tests}: if the bug is fixed, remove its entry"
    )


def test_parsers_see_entries_and_pins():
    fuzz = """
bool isKnownFinding(const InfomapWrapper& im, const char* message)
{
  // #1207: hard partitions.
  if (im.clusterDataIsHard) {
    return true; // #1209 too
  }
  return false;
}
// #1300 is outside the function.
"""
    tests = r"""
TEST_CASE("Fuzz finding: one (#1207) [fast]" * doctest::should_fail())
TEST_CASE("Fixed (#1209) [fast]")
// TEST_CASE("Commented out (#1210) [fast]" * doctest::should_fail())
TEST_CASE("Marker commented out (#1211) [fast]") // * doctest::should_fail()
TEST_CASE("Marker in a block comment (#1212) [fast]" /* * doctest::should_fail() */)
/* TEST_CASE("Block (#1213)" * doctest::should_fail()) */
TEST_CASE("A // in the name, \"quoted\" (#1214) [fast]" * doctest::should_fail())
"""
    assert known_issues(fuzz) == {"1207", "1209"}
    assert pinned_issues(tests) == {"1207", "1214"}
