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

This check compares each declared binding default -- for *every* binding
language present in the entry (Python and R today) -- against the engine
default from ``--print-json-parameters``. Value-typed parameters must match
numerically; the handful of boolean *flags* whose binding default intentionally
diverges (tri-state ``None``, integer verbosity, quiet-by-default) are listed
below with the reason, so an accidental drift on any other parameter still
fails loud. A tri-state "unset" literal (Python ``None``, R ``NULL``) carries
no scalar to compare and is skipped -- that is how a parameter opts a language
out of the engine default (e.g. R ``NULL`` for the multilayer relax limits).

It also checks *completeness* on the Python surface: every numeric value-typed
parameter must either declare a 'defaults' entry or be listed as intentionally
optional. The drift comparison alone iterates only the 'defaults' entries that
already exist, so it cannot catch a numeric parameter that was never added --
one that would silently render as ``T | None = None`` and drop its documented
engine default.

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

# Numeric value-typed parameters that intentionally render as an unset ``None``
# on the Python surface (``param: int | None = None``) instead of surfacing the
# engine default, so they are deliberately absent from overrides 'defaults'.
# Each entry documents why. The completeness check below fails loud on any
# *other* numeric parameter missing from 'defaults': that one would silently
# render as ``T | None = None`` and drop its documented engine default. Adding a
# numeric parameter to the engine therefore forces a conscious choice -- give it
# a 'defaults' entry, or list it here as intentionally optional.
INTENTIONAL_OPTIONAL_VALUE_PARAMS = {
    "--weight-threshold": "Optional link-weight cutoff; unset means no filtering.",
    "--node-limit": "Optional node-id cap; unset reads all nodes.",
    "--matchable-multilayer-ids": "Optional; unset disables cross-network state-id matching.",
    "--clu-level": "Optional depth; unset uses the engine default level.",
    "--trial-offset": "Optional distributed-shard offset; unset is single-process.",
    "--preferred-number-of-modules": "Optional soft preference; unset disables it.",
    "--preferred-number-of-levels": "Optional soft preference; unset disables it.",
    "--core-level-limit": "Optional reapply cap; unset uses the engine default.",
    "--tune-iteration-limit": "Optional iteration cap; unset uses the engine default.",
    "--num-random-moves": "Optional; unset disables random-move merging.",
    "--max-degree-for-random-moves": "Optional; unset means no degree cap.",
}

# Parameters that exist only in feature-enabled builds (guarded by
# ``#if INFOMAP_FEATURE_*`` in src/io/ParameterCatalog.cpp). The bindings are
# generated from a standard build that omits them, so they are deliberately
# outside the binding surface and carry no overrides entries -- but the CI
# 'native' binary may enable the feature, so ``--print-json-parameters`` lists
# them here. Exclude them from the completeness check; a new feature parameter
# that reaches this list fails loud until it is added (or genuinely bound).
FEATURE_GATED_PARAMS = {
    "--lossy",
    "--lambda",
    "--test-feature",
}

# The engine longTypes that render as a numeric Python default (int/float).
_NUMERIC_LONG_TYPES = {"integer", "number", "probability"}


def _as_bool(value: str) -> bool:
    return value.strip().lower() in {"true", "1"}


# Per-language "unset" literals: a tri-state default that carries no scalar to
# compare against the engine, so the parameter opts that language out of the
# engine default.
_UNSET_LITERALS = {"python": {"None"}, "r": {"NULL"}}


def _numeric(language: str, value: str) -> float:
    """Parse a language default literal as a float (R integer suffix aside)."""
    literal = value.strip()
    if language == "r" and literal.endswith("L"):
        literal = literal[:-1]
    return float(literal)


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
    catalog_obj = ParameterCatalog(parameters, overrides)

    drift: list[str] = []

    # Completeness: every numeric value-typed parameter rendered on the Python
    # surface must either declare a 'defaults' entry (checked below for drift)
    # or be listed as intentionally optional. Without this, a newly added engine
    # parameter that nobody wired into 'defaults' silently renders as
    # ``T | None = None`` -- its documented default lost -- and the drift loop
    # (which only iterates *existing* 'defaults' entries) cannot see the gap.
    python_hidden = catalog_obj.hidden_bindings.get("python", set())
    python_defaults = {
        flag for flag, langs in overrides["defaults"].items() if "python" in langs
    }
    for param in catalog_obj.parameters:
        if param.flag in FEATURE_GATED_PARAMS:
            continue  # feature-build-only; outside the standard binding surface
        if param.render_policy != "value":
            continue  # flags and special render policies handle defaults elsewhere
        if param.long_type not in _NUMERIC_LONG_TYPES:
            continue  # string/path values legitimately default to None (unset)
        if param.flag in python_hidden:
            continue  # not part of the generated Python signature
        if param.flag in python_defaults:
            continue  # concrete default surfaced; drift loop covers it
        if param.flag in INTENTIONAL_OPTIONAL_VALUE_PARAMS:
            continue
        drift.append(
            f"{param.flag} [python]: numeric value parameter has no 'defaults' "
            "entry and is not listed as intentionally optional; it would render "
            "as 'T | None = None' and drop its engine default"
        )
    for flag, per_language in overrides["defaults"].items():
        param = catalog.get(flag)
        if param is None:
            drift.append(
                f"{flag}: declared in overrides 'defaults' but not in the engine catalog"
            )
            continue
        engine_default = str(param.get("default"))
        # Value-typed parameters carry a longType (integer/number/probability);
        # boolean flags have none.
        is_flag = param.get("longType") is None
        for language, binding_default in per_language.items():
            if binding_default.strip() in _UNSET_LITERALS.get(language, set()):
                # Tri-state "unset": no scalar to compare (e.g. R NULL).
                continue
            if is_flag:
                if flag in INTENTIONAL_FLAG_DIVERGENCES:
                    continue
                if _as_bool(binding_default) != _as_bool(engine_default):
                    drift.append(
                        f"{flag} [{language}]: engine default {engine_default!r} != "
                        f"binding default {binding_default!r}"
                    )
                continue
            try:
                matches = _numeric(language, binding_default) == float(engine_default)
            except ValueError:
                matches = False
            if not matches:
                drift.append(
                    f"{flag} [{language}]: engine default {engine_default!r} != "
                    f"binding default {binding_default!r}"
                )

    if drift:
        print("Binding defaults drifted from engine defaults:", file=sys.stderr)
        for line in drift:
            print(f"  - {line}", file=sys.stderr)
        print(
            "\nUpdate interfaces/parameters/overrides.json 'defaults' to match the "
            "engine, or add an intentional divergence to INTENTIONAL_FLAG_DIVERGENCES "
            "/ INTENTIONAL_OPTIONAL_VALUE_PARAMS in this script with a reason.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
