"""Out-of-range numeric options raise a clear ValueError, not the engine's
misleading ``NetworkParseError: Cannot parse '<value>' ...``.

The engine's argument parser rejects an out-of-domain value (seed < 1, a
probability outside [0, 1], a negative markov time) with the same "Cannot
parse" message it uses for genuinely unparseable text -- even though the value
is a valid number that only violates the option's range, and the error surfaces
as ``NetworkParseError`` despite no network being parsed. The generated
``_validate_option_domains`` (from the catalog's inclusive min/max bounds)
catches these at the ``to_args`` render funnel first, naming the option and its
range. These tests pin that, the inclusive boundaries, and that every entry
point (functional ``run``, ``Options``, the stateful ``Infomap``, ``Network``)
is covered.
"""

from __future__ import annotations

import pytest

from infomap import Infomap, Network, Options, datasets, run
from infomap._options import _OPTION_DOMAINS, _validate_option_domains

pytestmark = pytest.mark.fast

LINKS = [(1, 2), (2, 3), (3, 1)]


def _net():
    net = Network()
    for u, v in LINKS:
        net.add_link(u, v)
    return net


@pytest.mark.parametrize(
    "kwargs, needle",
    [
        ({"seed": 0}, "seed must be >= 1"),
        ({"seed": -1}, "seed must be >= 1"),
        ({"num_trials": 0}, "num_trials must be >= 1"),
        ({"num_trials": -3}, "num_trials must be >= 1"),
        ({"markov_time": -1.0}, "markov_time must be >= 0"),
    ],
)
def test_run_common_tier_out_of_range_raises_valueerror(kwargs, needle):
    with pytest.raises(ValueError, match=needle):
        run(LINKS, **kwargs)


def test_probability_out_of_range_reports_both_bounds():
    with pytest.raises(ValueError, match=r"between 0.0 and 1.0"):
        run(LINKS, options=Options(teleportation_probability=2.0))


def test_out_of_range_is_not_networkparseerror():
    # The whole point: the old behaviour surfaced this as NetworkParseError with
    # a "Cannot parse" message. It must now be a plain ValueError.
    from infomap import NetworkParseError

    with pytest.raises(ValueError) as excinfo:
        run(LINKS, seed=0)
    assert not isinstance(excinfo.value, NetworkParseError)
    assert "Cannot parse" not in str(excinfo.value)


@pytest.mark.parametrize(
    "factory",
    [
        pytest.param(lambda: Options(seed=0).to_args(), id="options-to-args"),
        pytest.param(lambda: Infomap(seed=0), id="infomap-init"),
        pytest.param(lambda: _net().run(options=Options(seed=0)), id="network-run"),
        pytest.param(lambda: run(LINKS, options=Options(seed=0)), id="run-options"),
    ],
)
def test_every_entry_point_validates(factory):
    with pytest.raises(ValueError, match="seed must be >= 1"):
        factory()


@pytest.mark.parametrize(
    "options",
    [
        Options(markov_time=0.0),  # min is inclusive
        Options(teleportation_probability=0.0),
        Options(teleportation_probability=1.0),  # max is inclusive
        Options(seed=1),
        Options(num_trials=1),
    ],
)
def test_inclusive_boundaries_are_accepted(options):
    # Boundary values equal to a bound must render without error (mirrors the
    # C++ parser rejecting only value < min or value > max).
    options.to_args()
    run(datasets.two_triangles(), options=options)


def test_defaults_are_all_in_range():
    # Every option's own default must satisfy its domain, or plain run() would
    # raise before touching the engine.
    _validate_option_domains(Options().to_kwargs())


def test_domain_table_matches_catalog_bounds():
    # The generated _OPTION_DOMAINS must reflect the engine's declared min/max,
    # so validation can never drift from what the engine actually enforces.
    import json
    import subprocess
    import sys
    from pathlib import Path

    repo_root = Path(__file__).resolve().parents[2]
    binary = repo_root / "Infomap"
    if not binary.exists():
        pytest.skip("Infomap binary not built")
    sys.path.insert(0, str(repo_root / "scripts"))
    from parameter_catalog import snake_name

    parameters = json.loads(
        subprocess.run(
            [str(binary), "--print-json-parameters"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
    )["parameters"]
    for param in parameters:
        if "min" not in param and "max" not in param:
            continue
        if param.get("longType") not in {"integer", "number", "probability"}:
            continue
        name = snake_name(param["long"])
        if name not in _OPTION_DOMAINS:
            continue  # special render policy (repeated short, output list, ...)
        low, high = _OPTION_DOMAINS[name]
        is_int = param["longType"] == "integer"
        if "min" in param:
            expected = int(param["min"]) if is_int else float(param["min"])
            assert low == expected, param["long"]
        else:
            assert low is None, param["long"]
        if "max" in param:
            expected = int(param["max"]) if is_int else float(param["max"])
            assert high == expected, param["long"]
        else:
            assert high is None, param["long"]
