#!/usr/bin/env python3
"""Guard: ``interfaces/parameters/overrides.json`` binding defaults must track
the C++ engine defaults.

The Python/R signature generator renders the per-language default from
``overrides.json`` and omits a flag when the caller's value equals that
default. If a binding default silently drifts from the engine default, the
generated signature documents (and passes through) the wrong value: the flag
is not rendered, so the engine quietly uses a different default. The byte-level
freshness check cannot catch this because it regenerates identical output from
the unchanged override.

This check compares each declared binding default against the engine default
from ``--print-json-parameters``. Value-typed parameters must match
numerically; the handful of boolean *flags* whose binding default intentionally
diverges (tri-state ``None``, integer verbosity, quiet-by-default) are listed
below with the reason, so an accidental drift on any other parameter still
fails loud.

Usage:  check_binding_defaults.py <path-to-Infomap-binary>
"""

import json
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
OVERRIDES = REPO_ROOT / "interfaces" / "parameters" / "overrides.json"

sys.path.insert(0, str(REPO_ROOT / "scripts"))

from parameter_catalog import ParameterCatalog  # noqa: E402

# Boolean C++ flags whose Python binding default intentionally diverges from
# the engine's ``false``. Each entry documents why; everything else must match.
INTENTIONAL_FLAG_DIVERGENCES = {
    "--silent": "Python API is quiet by default (True); the C++/CLI default is False.",
    "--directed": "Tri-state None (unset) in the API vs the CLI boolean flag.",
    "--fast-hierarchical-solution": "Optional int (None = unset) vs the CLI repeated -F flag.",
    "--verbose": "Integer verbosity_level (default level 1) vs the CLI repeated -v flag.",
}


def _as_bool(value: str) -> bool:
    return value.strip().lower() in {"true", "1"}


def main() -> int:
    cli = sys.argv[1]
    result = subprocess.run(
        [cli, "--print-json-parameters"],
        check=True,
        capture_output=True,
        text=True,
    )
    parameters = json.loads(result.stdout)["parameters"]
    catalog = {param["long"]: param for param in parameters}
    overrides = json.loads(OVERRIDES.read_text(encoding="utf-8"))

    # Constructing the catalog runs _validate_policy against the *real* engine
    # flags, so a policy entry (per-parameter, group, or cliOnlyParameters)
    # naming a stale or misspelled flag fails here in the fast tier -- the unit
    # tests can only synthesize the catalog from the policy's own flag set.
    ParameterCatalog(parameters, overrides)

    drift: list[str] = []
    for flag, per_language in overrides["defaults"].items():
        param = catalog.get(flag)
        if param is None:
            drift.append(
                f"{flag}: declared in overrides 'defaults' but not in the engine catalog"
            )
            continue
        engine_default = str(param.get("default"))
        python_default = per_language["python"]
        # Value-typed parameters carry a longType (integer/number/probability);
        # boolean flags have none.
        if param.get("longType") is None:
            if flag in INTENTIONAL_FLAG_DIVERGENCES:
                continue
            if _as_bool(python_default) != _as_bool(engine_default):
                drift.append(
                    f"{flag}: engine default {engine_default!r} != binding default "
                    f"{python_default!r}"
                )
            continue
        if float(engine_default) != float(python_default):
            drift.append(
                f"{flag}: engine default {engine_default!r} != binding default "
                f"{python_default!r}"
            )

    if drift:
        print("Binding defaults drifted from engine defaults:", file=sys.stderr)
        for line in drift:
            print(f"  - {line}", file=sys.stderr)
        print(
            "\nUpdate interfaces/parameters/overrides.json 'defaults' to match the "
            "engine, or add an intentional divergence to INTENTIONAL_FLAG_DIVERGENCES "
            "in this script with a reason.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
