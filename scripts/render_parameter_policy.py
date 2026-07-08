#!/usr/bin/env python3
"""Render the 3.0 parameter-policy surface matrix (issue #755) as Markdown.

Reads the same inputs as the binding generator -- the C++ parameter catalog
(``./Infomap --print-json-parameters``) and ``interfaces/parameters/
overrides.json`` -- and prints one row per parameter with its policy decision
on every surface. Replacement guidance is listed below the table, one bullet
per non-keep decision, so every removed or hidden parameter documents its
migration path (the #755 acceptance criterion).

Usage:  python scripts/render_parameter_policy.py [--binary ./Infomap]
        (or: make print-parameter-policy)
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from parameter_catalog import GROUPS, ParameterCatalog  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[1]
OVERRIDES_PATH = REPO_ROOT / "interfaces" / "parameters" / "overrides.json"

SURFACE_LABELS = {"cli": "CLI", "python": "Python", "r": "R", "ts": "JS"}


def load_catalog(binary: str) -> ParameterCatalog:
    output = subprocess.run(
        [binary, "--print-json-parameters"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    parameters = json.loads(output)["parameters"]
    overrides = json.loads(OVERRIDES_PATH.read_text(encoding="utf-8"))
    return ParameterCatalog(parameters, overrides)


def format_action(decision: dict[str, str]) -> str:
    action = decision["action"]
    if action == "alias":
        return f"alias of `{decision['aliasOf']}`"
    return action


def render(catalog: ParameterCatalog) -> str:
    policy = catalog.overrides.get("policy", {})
    surfaces = policy.get("surfaces", ["cli", "python", "r", "ts"])
    lines: list[str] = []
    notes: list[str] = []

    lines.append("# Infomap 3.0 parameter policy (#755)")
    lines.append("")
    lines.append("Actions: " + "; ".join(
        f"**{name}** = {text}" for name, text in policy.get("vocabulary", {}).items()
    ))
    lines.append("")
    header = "| Parameter | Group | " + " | ".join(
        SURFACE_LABELS[s] for s in surfaces
    ) + " |"
    lines.append(header)
    lines.append("|" + "---|" * (2 + len(surfaces)))

    def add_row(flag: str, group: str, decisions: dict[str, dict[str, str]]) -> None:
        cells = []
        for surface in surfaces:
            decision = decisions[surface]
            cell = format_action(decision)
            if decision["action"] != "keep":
                cell = f"**{cell}**"
                notes.append(
                    f"- `{flag}` ({SURFACE_LABELS[surface]}, "
                    f"{decision['action']}): {decision['replacement']}"
                )
            cells.append(cell)
        lines.append(f"| `{flag}` | {group} | " + " | ".join(cells) + " |")

    for group in GROUPS:
        for param in catalog.grouped()[group]:
            add_row(
                param.flag,
                group,
                {surface: param.policy(surface) for surface in surfaces},
            )

    # Parameters outside the C++ binding catalog: binding-only compatibility
    # parameters and hidden CLI-only flags still need policy rows.
    extra_flags: dict[str, str] = {}
    for language, entries in catalog.binding_only_parameters.items():
        for entry in entries:
            if entry.flag.startswith("--") and entry.flag not in extra_flags:
                extra_flags[entry.flag] = f"bindingOnly ({language})"
    for flag in policy.get("cliOnlyParameters", []):
        extra_flags.setdefault(flag, "CLI-only (hidden)")

    parameters_policy = policy.get("parameters", {})
    default_action = {"action": policy.get("default", "keep")}
    for flag, group in sorted(extra_flags.items()):
        add_row(
            flag,
            group,
            {
                surface: parameters_policy.get(flag, {}).get(surface, default_action)
                for surface in surfaces
            },
        )

    lines.append("")
    lines.append("## Replacement guidance")
    lines.append("")
    lines.extend(notes)
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default=str(REPO_ROOT / "Infomap"))
    args = parser.parse_args()
    print(render(load_catalog(args.binary)))


if __name__ == "__main__":
    main()
