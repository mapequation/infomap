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

from parameter_catalog import (  # noqa: E402
    GROUPS,
    ParameterCatalog,
    resolve_policy_decision,
)

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

    lines.append("# Infomap 3.0 parameter policy")
    lines.append("")
    lines.append(
        "Actions: "
        + "; ".join(
            f"**{name}** = {text}"
            for name, text in policy.get("vocabulary", {}).items()
        )
    )
    lines.append("")
    header = (
        "| Parameter | Group | "
        + " | ".join(SURFACE_LABELS[s] for s in surfaces)
        + " |"
    )
    lines.append(header)
    lines.append("|" + "---|" * (2 + len(surfaces)))

    def add_row(
        flag: str,
        group: str,
        decisions: dict[str, dict[str, str]],
        exists_on: set[str],
    ) -> None:
        cells = []
        for surface in surfaces:
            if surface not in exists_on:
                cells.append("—")
                continue
            decision = decisions[surface]
            cell = format_action(decision)
            if decision.get("tier") == "common":
                cell += " (common tier)"
            if decision["action"] != "keep":
                cell = f"**{cell}**"
                # Surface the per-language binding name when it differs from the
                # CLI flag (e.g. --verbose -> Python/R verbosity_level), so
                # someone searching the policy by the name they type in code
                # finds the entry instead of only the CLI spelling.
                rename = catalog.overrides.get("names", {}).get(flag, {}).get(surface)
                suffix = (
                    f" ({SURFACE_LABELS[surface]} name: `{rename}`)" if rename else ""
                )
                notes.append(
                    f"- `{flag}` ({SURFACE_LABELS[surface]}, "
                    f"{decision['action']}): {decision['replacement']}{suffix}"
                )
            cells.append(cell)
        lines.append(f"| `{flag}` | {group} | " + " | ".join(cells) + " |")

    all_surfaces = set(surfaces)
    for group in GROUPS:
        for param in catalog.grouped()[group]:
            add_row(
                param.flag,
                group,
                {surface: param.policy(surface) for surface in surfaces},
                exists_on=all_surfaces,
            )

    # Parameters outside the C++ binding catalog exist only where declared:
    # binding-only compatibility parameters on their languages (they are also
    # real CLI flags -- accepted, erroring, or hidden), and CLI-only flags on
    # the CLI alone. Other cells render as "—".
    extra_flags: dict[str, tuple[str, set[str]]] = {}
    for language, entries in catalog.binding_only_parameters.items():
        for entry in entries:
            if not entry.flag.startswith("--"):
                continue
            group_label, exists_on = extra_flags.get(
                entry.flag, ("bindingOnly", {"cli"})
            )
            exists_on = exists_on | {language}
            extra_flags[entry.flag] = (group_label, exists_on)
    for flag in policy.get("cliOnlyParameters", []):
        extra_flags.setdefault(flag, ("CLI-only (hidden)", {"cli"}))

    for flag, (group_label, exists_on) in sorted(extra_flags.items()):
        add_row(
            flag,
            group_label,
            {
                surface: resolve_policy_decision(catalog.overrides, flag, surface)
                for surface in surfaces
            },
            exists_on=exists_on,
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
