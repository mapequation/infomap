#!/usr/bin/env python3

import argparse
import json
import shutil
from pathlib import Path


KEEP_PACKAGE_FIELDS = [
    "name",
    "version",
    "description",
    "browser",
    "publishConfig",
    "files",
    "main",
    "types",
    "type",
    "repository",
    "keywords",
    "author",
    "maintainers",
    "license",
    "bugs",
    "homepage",
]


def copy_file(source, target_dir, target_name=None):
    source_path = Path(source)
    target_path = Path(target_dir) / (target_name or source_path.name)
    shutil.copy2(source_path, target_path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-package", default="package.json")
    parser.add_argument("--readme", default="interfaces/js/README.md")
    parser.add_argument("--readme-rst", default="README.rst")
    parser.add_argument("--license", default="LICENSE_GPLv3.txt")
    parser.add_argument("--output-dir", default="dist/npm/package")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    source_package = json.loads(Path(args.source_package).read_text(encoding="utf-8"))
    staged_package = {key: source_package[key] for key in KEEP_PACKAGE_FIELDS if key in source_package}
    (output_dir / "package.json").write_text(json.dumps(staged_package, indent=2) + "\n", encoding="utf-8")

    copy_file(args.readme, output_dir, "README.md")
    copy_file(args.readme_rst, output_dir, "README.rst")
    copy_file(args.license, output_dir, "LICENSE_GPLv3.txt")


if __name__ == "__main__":
    main()
