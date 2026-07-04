"""Guard the Python documentation's terminology decisions.

A companion to the writing review: once a term is settled, this keeps it settled
so the choices do not quietly drift back over future edits. It is deliberately
narrow — only unambiguous, false-positive-free rules live here, so a failure
always means a real regression, never a judgement call.

Rules
-----
* ``meta data`` (with a space) must be written ``metadata``. The API identifiers
  ``meta_data`` / ``meta_data_rate`` / ``set_meta_data`` use underscores and are
  left untouched (the space is what the rule keys on).
* In prose, the Scanpy project is capitalised ``Scanpy``. Lowercase ``scanpy`` is
  fine as a code identifier, module path, or doc-reference target, so those are
  stripped before the check.

Run: ``python scripts/check_docs_terminology.py`` (exit 1 on any violation).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DOCS_SOURCE = REPO_ROOT / "interfaces" / "python" / "source"
PY_PACKAGE = REPO_ROOT / "interfaces" / "python" / "src" / "infomap"

# SWIG-generated; edit the .i interface file instead, so exclude from the sweep.
GENERATED = {PY_PACKAGE / "_swig.py"}

# \s spans newlines so a "meta data" wrapped across a line break is caught too;
# the underscore in the API names meta_data / meta_data_rate is not whitespace,
# so those never match.
META_DATA = re.compile(r"meta\s+data", re.IGNORECASE)
SCANPY_LOWER = re.compile(r"\bscanpy\b")

_FENCE = re.compile(r"^\s*(```|~~~)")
_INLINE_CODE = re.compile(r"`[^`]*`")
_ANGLE_TARGET = re.compile(r"<[^>]*>")  # {doc}`Label <target>` reference targets
_URL = re.compile(r"https?://\S+")


def _strip_code(text: str) -> str:
    """Remove inline code, reference targets, and URLs so prose rules see prose."""
    text = _INLINE_CODE.sub("", text)
    text = _ANGLE_TARGET.sub("", text)
    return _URL.sub("", text)


def _prose_lines(path: Path) -> list[tuple[int, str]]:
    """Yield (line number, code-stripped line) for lines outside fenced blocks."""
    out: list[tuple[int, str]] = []
    in_fence = False
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if _FENCE.match(raw):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        out.append((lineno, _strip_code(raw)))
    return out


def _iter_files(root: Path, suffixes: tuple[str, ...]):
    for path in sorted(root.rglob("*")):
        if path.suffix in suffixes and path not in GENERATED:
            yield path


def main() -> int:
    violations: list[str] = []

    # Rule 1: "meta data" -> "metadata", across docs prose and package docstrings.
    for path in [*_iter_files(DOCS_SOURCE, (".md", ".rst")),
                 *_iter_files(PY_PACKAGE, (".py",))]:
        text = path.read_text(encoding="utf-8")
        for match in META_DATA.finditer(text):
            lineno = text.count("\n", 0, match.start()) + 1
            rel = path.relative_to(REPO_ROOT)
            violations.append(f'{rel}:{lineno}: write "metadata", not "meta data"')

    # Rule 2: prose "scanpy" -> "Scanpy" (code identifiers/targets excluded).
    for path in _iter_files(DOCS_SOURCE, (".md", ".rst")):
        for lineno, prose in _prose_lines(path):
            if SCANPY_LOWER.search(prose):
                rel = path.relative_to(REPO_ROOT)
                violations.append(f'{rel}:{lineno}: capitalise "Scanpy" in prose')

    if violations:
        print("Documentation terminology check failed:\n", file=sys.stderr)
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        print(f"\n{len(violations)} violation(s).", file=sys.stderr)
        return 1

    print("Documentation terminology check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
