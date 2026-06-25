"""Guard: the public surface of :class:`infomap.Infomap` must not change.

This freezes the set of public attribute names and, for members defined in our
own Python modules, their signatures. SWIG-inherited members (``infomap._swig``)
are pinned by name only, so the baseline stays robust across SWIG patch versions.

Regenerate the baseline (only when an *intentional, additive* change is made and
reviewed) with::

    python test/python/test_public_surface.py
"""

import inspect
import json
from pathlib import Path

import pytest

from infomap import Infomap

BASELINE = Path(__file__).parent / "public_surface_baseline.json"

# Members defined in these modules get their signatures pinned exactly.
OWNED_MODULES = {
    "infomap._facade",
    "infomap._results",
    "infomap._writers",
    "infomap.network",
    "infomap._summary",
}


def snapshot():
    surface = {}
    for name, member in inspect.getmembers(Infomap):
        if name.startswith("_"):
            continue
        attr = inspect.getattr_static(Infomap, name)
        is_property = isinstance(attr, property)
        target = attr.fget if is_property else member
        module = getattr(target, "__module__", "") or ""
        if module in OWNED_MODULES:
            try:
                sig = str(inspect.signature(target))
            except (TypeError, ValueError):
                sig = None
        else:
            sig = "<unpinned>"
        surface[name] = {"property": is_property, "signature": sig}
    return surface


@pytest.mark.fast
def test_public_surface_matches_baseline():
    current = snapshot()
    expected = json.loads(BASELINE.read_text(encoding="utf-8"))
    added = set(current) - set(expected)
    removed = set(expected) - set(current)
    changed = {
        k: (expected[k], current[k])
        for k in set(current) & set(expected)
        if current[k] != expected[k]
    }
    assert not (added or removed or changed), (
        "Public surface of Infomap changed (additive-only refactor must preserve it). "
        f"Added={sorted(added)} Removed={sorted(removed)} Changed={changed}"
    )


if __name__ == "__main__":
    BASELINE.write_text(
        json.dumps(snapshot(), indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"Wrote {BASELINE}")
