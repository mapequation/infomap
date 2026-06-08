from __future__ import annotations

import json
from functools import cache
from pathlib import Path
from typing import Any

from referencing import Registry, Resource
from referencing.jsonschema import DRAFT202012
from jsonschema import Draft202012Validator


def _repo_root() -> Path:
    current = Path(__file__).resolve()
    for parent in current.parents:
        if (parent / "pyproject.toml").exists() and (parent / "src").is_dir():
            return parent
    raise FileNotFoundError(f"Could not locate repository root from {current}")


def _schema_path(name: str) -> Path:
    return _repo_root() / "test" / "schemas" / "json" / name


def _schema_dir() -> Path:
    return _repo_root() / "test" / "schemas" / "json"


def load_schema(name: str) -> dict[str, Any]:
    return json.loads(_schema_path(name).read_text(encoding="utf-8"))


@cache
def _registry() -> Registry:
    resources = []
    for path in _schema_dir().glob("*.schema.json"):
        schema = json.loads(path.read_text(encoding="utf-8"))
        resources.append(
            (
                schema["$id"],
                Resource.from_contents(schema, default_specification=DRAFT202012),
            )
        )
    return Registry().with_resources(resources)


def validate_json_schema(instance: Any, schema_name: str) -> None:
    schema = load_schema(schema_name)
    Draft202012Validator.check_schema(schema)
    validator = Draft202012Validator(schema, registry=_registry())
    errors = sorted(validator.iter_errors(instance), key=lambda error: list(error.path))
    if not errors:
        return

    lines = [f"{len(errors)} JSON schema validation error(s) for {schema_name}:"]
    for error in errors[:20]:
        path = "$"
        for part in error.path:
            path += f"[{part}]" if isinstance(part, int) else f".{part}"
        lines.append(f"- {path}: {error.message}")
    if len(errors) > 20:
        lines.append(f"- ... {len(errors) - 20} more")
    raise AssertionError("\n".join(lines))
