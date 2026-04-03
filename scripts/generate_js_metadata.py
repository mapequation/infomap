#!/usr/bin/env python3

import argparse
import json
import re
import subprocess
from pathlib import Path


RELEASE_HEADING = re.compile(
    r"^#{2,3}\s+\[(?P<version>[^\]]+)\]\([^)]+\)\s+\((?P<date>\d{4}-\d{2}-\d{2})\)\s*$"
)
SECTION_HEADING = re.compile(r"^###\s+(?P<section>Features|Bug Fixes|⚠ BREAKING CHANGES)\s*$")
MARKDOWN_LINK = re.compile(r"\[([^\]]+)\]\([^)]+\)")
ISSUE_REF = re.compile(r"#\d+")
COMMIT_REF = re.compile(r"\b[0-9a-f]{7,40}\b")


def strip_links(text):
    return MARKDOWN_LINK.sub(r"\1", text)


def parse_subject(markdown_line):
    cleaned = strip_links(markdown_line).strip()
    scoped = re.match(r"^\*\s+\*\*(?P<scope>[^*]+):\*\*\s+(?P<subject>.+)$", cleaned)
    if scoped:
        return scoped.group("scope").strip(), scoped.group("subject").strip()
    plain = re.match(r"^\*\s+(?P<subject>.+)$", cleaned)
    if plain:
        return None, plain.group("subject").strip()
    return None, cleaned.removeprefix("*").strip()


def section_type(section):
    if section == "Features":
        return "feat"
    if section == "Bug Fixes":
        return "fix"
    if section == "⚠ BREAKING CHANGES":
        return "breaking"
    return None


def parse_changelog(changelog_path):
    entries = []
    current_release = None
    current_section = None

    for line in changelog_path.read_text(encoding="utf-8").splitlines():
        release_match = RELEASE_HEADING.match(line)
        if release_match:
            current_release = release_match.groupdict()
            current_section = None
            continue

        section_match = SECTION_HEADING.match(line)
        if section_match:
            current_section = section_match.group("section")
            continue

        if not current_release or not current_section or not line.startswith("* "):
            continue

        scope, subject = parse_subject(line)
        type_name = section_type(current_section)
        references = sorted(set(ISSUE_REF.findall(line) + COMMIT_REF.findall(line)))
        header_scope = f"({scope})" if scope else ""
        header_type = type_name if type_name and type_name != "breaking" else "note"
        header = f"{header_type}{header_scope}: {subject}"
        notes = [subject] if current_section == "⚠ BREAKING CHANGES" else []

        entries.append(
            {
                "body": None,
                "date": current_release["date"],
                "footer": f'{current_release["version"]} ({current_release["date"]})',
                "header": header,
                "mentions": [],
                "merge": None,
                "notes": notes,
                "references": references,
                "revert": None,
                "scope": scope,
                "subject": subject,
                "type": type_name,
            }
        )

    return entries


def load_parameters(infomap_bin):
    infomap_bin = infomap_bin.resolve()
    completed = subprocess.run(
        [str(infomap_bin), "--print-json-parameters"],
        check=True,
        capture_output=True,
        text=True,
    )
    payload = json.loads(completed.stdout)
    return payload["parameters"]


def write_json(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--infomap-bin", required=True)
    parser.add_argument("--changelog", default="CHANGELOG.md")
    parser.add_argument("--output-dir", default="interfaces/js/generated")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    changelog = parse_changelog(Path(args.changelog))
    parameters = load_parameters(Path(args.infomap_bin))

    write_json(output_dir / "changelog.json", changelog)
    write_json(output_dir / "parameters.json", parameters)


if __name__ == "__main__":
    main()
