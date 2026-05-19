#!/usr/bin/env python3
"""Verify tracked JavaScript output metadata against the C++ manifest."""

import json
import re
import sys
from pathlib import Path


MIME_TYPES = {
    "textMimeType": "text/plain;charset=utf-8",
    "jsonMimeType": "application/json;charset=utf-8",
}

FORMAT_PATTERN = re.compile(r'^\s*format\("([^"]+)",')
FILE_PATTERN = re.compile(
    r'^\s*file\("([^"]+)", "([^"]+)", (true|false), "([^"]*)", "([^"]+)", (textMimeType|jsonMimeType)\)'
)


def read_cpp_manifest(source: Path) -> list[dict[str, object]]:
    formats: list[dict[str, object]] = []
    current: dict[str, object] | None = None

    for line in source.read_text().splitlines():
        format_match = FORMAT_PATTERN.match(line)
        if format_match:
            current = {"optionName": format_match.group(1), "files": []}
            formats.append(current)
            continue

        file_match = FILE_PATTERN.match(line)
        if file_match and current is not None:
            files = current["files"]
            assert isinstance(files, list)
            files.append(
                {
                    "key": file_match.group(1),
                    "name": file_match.group(2),
                    "isStates": file_match.group(3) == "true",
                    "suffix": file_match.group(4),
                    "extension": file_match.group(5),
                    "mimeType": MIME_TYPES[file_match.group(6)],
                }
            )

    return formats


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: check_js_output_formats.py OUTPUT_FORMATS_CPP OUTPUT_FORMATS_JSON")
        return 2

    cpp_formats = read_cpp_manifest(Path(sys.argv[1]))
    js_manifest = json.loads(Path(sys.argv[2]).read_text())

    if js_manifest["formats"] != cpp_formats:
        print("JavaScript output format metadata differs from src/io/OutputFormats.cpp")
        return 1

    result_keys = [file["key"] for format in cpp_formats for file in format["files"]]
    if sorted(js_manifest["resultOrder"]) != sorted(result_keys):
        print("JavaScript resultOrder must contain each C++ result key exactly once")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
