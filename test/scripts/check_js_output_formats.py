#!/usr/bin/env python3
"""Verify tracked JavaScript output metadata against the C++ manifest."""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from schema_validation import validate_json_schema


def read_cpp_manifest(source: Path) -> dict[str, object]:
    repo_root = source.parents[2]

    with tempfile.TemporaryDirectory() as temp_dir_name:
        temp_dir = Path(temp_dir_name)
        helper = temp_dir / "emit_output_formats.cpp"
        binary = temp_dir / "emit_output_formats"
        helper.write_text(
            '#include "io/OutputFormats.h"\n'
            "#include <iostream>\n"
            "int main()\n"
            "{\n"
            "  std::cout << infomap::outputFormatsManifestJson();\n"
            "  return 0;\n"
            "}\n"
        )

        compiler = os.environ.get("CXX", "c++")
        subprocess.run(
            [
                compiler,
                "-std=c++14",
                "-I",
                str(repo_root / "src"),
                "-I",
                str(repo_root / "vendor" / "nlohmann_json" / "include"),
                str(helper),
                str(source),
                "-o",
                str(binary),
            ],
            check=True,
        )
        return json.loads(subprocess.check_output([str(binary)], text=True))


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "Usage: check_js_output_formats.py OUTPUT_FORMATS_CPP OUTPUT_FORMATS_JSON"
        )
        return 2

    cpp_manifest = read_cpp_manifest(Path(sys.argv[1]))
    js_manifest = json.loads(Path(sys.argv[2]).read_text())
    validate_json_schema(cpp_manifest, "output-formats-manifest.schema.json")
    validate_json_schema(js_manifest, "output-formats-manifest.schema.json")

    if js_manifest != cpp_manifest:
        print("JavaScript output format metadata differs from src/io/OutputFormats.cpp")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
