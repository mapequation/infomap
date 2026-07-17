"""Guard: the hand-written ``infomap.run()`` signature must surface exactly the
catalog's common-tier options as explicit keyword parameters.

Unlike ``Infomap.__init__`` / ``Infomap.run`` -- whose full keyword signatures
are GENERATED into a marker block in ``_facade.py`` from the parameter catalog
and checked by ``test_signature_catalog.py`` plus
``make test-binding-options-freshness`` -- the functional front door
``infomap.run`` is authored by hand in ``_run.py``. It surfaces only the
common-tier options as first-class keyword parameters (for IDE/agent
introspection) and carries every other engine option through ``**overrides``.
Nothing regenerates that hand-written list, so if the catalog's common tier
changes it would silently drift from the stateful signatures. This test pins it
to the policy's common tier, the single source of truth.
"""

import inspect
import json
from pathlib import Path

import infomap
import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]

# run()'s own structural parameters, not engine options. ``args`` is the raw
# Infomap CLI escape hatch (full-parity arguments prepended before the rendered
# options), matching Infomap.run / Network.run -- a structural argument like
# ``options`` and ``initial_partition``, not a catalog option.
NON_OPTION_RUN = {"input", "options", "args", "initial_partition"}


def _common_tier_python_names() -> set[str]:
    overrides = json.loads(
        (REPO_ROOT / "interfaces" / "parameters" / "overrides.json").read_text(
            encoding="utf-8"
        )
    )
    names = {
        flag.removeprefix("--").replace("-", "_")
        for flag, per_surface in overrides["policy"]["parameters"].items()
        if per_surface.get("python", {}).get("tier") == "common"
    }
    assert names, "no common-tier parameters found in the policy"
    return names


@pytest.mark.fast
def test_run_surfaces_exactly_the_common_tier_options():
    if not (REPO_ROOT / "interfaces" / "parameters" / "overrides.json").is_file():
        pytest.skip("parameter policy not available (wheel/sdist test run)")

    explicit = {
        name
        for name, param in inspect.signature(infomap.run).parameters.items()
        if param.kind
        in (
            inspect.Parameter.POSITIONAL_OR_KEYWORD,
            inspect.Parameter.KEYWORD_ONLY,
        )
    } - NON_OPTION_RUN

    common = _common_tier_python_names()
    assert explicit == common, (
        "infomap.run() explicit keyword options drifted from the catalog's "
        "common tier. Edit the policy in interfaces/parameters/overrides.json "
        "and the run() signature in _run.py together.\n"
        f"On run() but not common tier={sorted(explicit - common)}; "
        f"in common tier but not on run()={sorted(common - explicit)}"
    )
