"""Schema parity tests for the infomap-network-json v1.0 input format (RFC #645).

These validate the JSON Schema's accept/reject set. The SAX parser implemented
in later phases must match this set; the same fixtures are reused as parser
inputs to prove parity.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

SCHEMA_NAME = "infomap-network-json.schema.json"

_FIXTURE_DIR = Path(__file__).resolve().parents[1] / "fixtures" / "networks" / "json"
_VALID = sorted(_FIXTURE_DIR.glob("*.json"))
_INVALID = sorted((_FIXTURE_DIR / "invalid").glob("*.json"))


def _load(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def test_fixtures_exist() -> None:
    assert _VALID, "expected valid JSON network fixtures"
    assert _INVALID, "expected invalid JSON network fixtures"


@pytest.mark.parametrize("path", _VALID, ids=lambda p: p.name)
def test_valid_fixtures_pass_schema(path: Path, validate_json_schema) -> None:
    validate_json_schema(_load(path), SCHEMA_NAME)


@pytest.mark.parametrize("path", _INVALID, ids=lambda p: p.name)
def test_invalid_fixtures_rejected_by_schema(path: Path, validate_json_schema) -> None:
    with pytest.raises(AssertionError):
        validate_json_schema(_load(path), SCHEMA_NAME)


def test_integral_doubles_accepted_non_integral_rejected(validate_json_schema) -> None:
    """Integer coercion: 10.0 is a valid id, 1.5 is not (RFC integer coercion)."""
    base = {"format": "infomap-network-json", "version": "1.0"}

    validate_json_schema(
        {**base, "edges": [{"source": 10.0, "target": 11}]}, SCHEMA_NAME
    )
    with pytest.raises(AssertionError):
        validate_json_schema(
            {**base, "edges": [{"source": 1.5, "target": 2}]}, SCHEMA_NAME
        )


def test_negative_edge_weight_is_valid_but_negative_node_weight_is_not(
    validate_json_schema,
) -> None:
    """Edge weights <= 0 are ignored by the core (not errors); node weights must be >= 0."""
    base = {"format": "infomap-network-json", "version": "1.0"}

    validate_json_schema(
        {**base, "edges": [{"source": 1, "target": 2, "weight": -1.0}]}, SCHEMA_NAME
    )
    with pytest.raises(AssertionError):
        validate_json_schema(
            {
                **base,
                "nodes": [{"id": 1, "weight": -1.0}],
                "edges": [{"source": 1, "target": 2}],
            },
            SCHEMA_NAME,
        )
