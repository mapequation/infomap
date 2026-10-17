"""Pairwise coverage of the option matrix, checked against the oracles (#1171).

Bugs cluster where two options meet. Running every combination of the options
below is out of reach, but a covering array that holds every pair of levels of
every two factors at least once is small. Each row of it is run on a few
fixtures and small generated networks, and each run is checked with every oracle
that covers it:

- the run finishes, or is refused with the error the table below expects, and it
  warns about exactly the options the table says it ignores;
- the flow invariants hold. ``checkInvariants()`` is compiled into debug,
  sanitizer and fuzz builds only; build the extension with
  ``CXXFLAGS=-DINFOMAP_CHECK_INVARIANTS=1`` to run them here, where a violation
  surfaces as an ``InfomapError``;
- the reported codelength is the codelength the independent reference
  (``reference_codelength``) gives for the returned tree;
- the tree written out and read back with ``--no-infomap`` scores the same;
- node and link flow match the independent flow oracle (``reference_flow``).

The default run checks every row on one network and one seed. The full sweep
runs with ``--run-slow``: every row on every network of its kind with two
seeds, for this array and nine more built with the levels in shuffled order.

Factors and levels
------------------
network          plain, bipartite, multilayer, state, meta
flow_model       undirected, directed, undirdir, outdirdir, rawdir, precomputed
markov_time      1, 0.5, 2, variable (``--variable-markov-time``)
search           one trial, three trials, three ``--parallel-trials``
cluster_data     none, soft (a seed), hard (``clusterDataIsHard``, C++ and
                 SWIG only, so it is set on the engine)
teleport_to      links, nodes
recorded         unrecorded, recorded teleportation
alpha            0.15, 0.3
levels           multilevel, two-level
preferred        none, 2 modules
entropy          off, ``--entropy-corrected``
regularized      off, ``--regularized``
node_weights     off, ``--use-node-weights-as-flow``
threads          1, 2

Refused by design
-----------------
Two kinds. An option the run ignores is warned about, and the run goes on
without it; those combinations stay in the array, and the oracles check the run
as the one without the option:

- ``--regularized`` under undirdir, outdirdir, rawdir and precomputed (#1196);
- ``--recorded-teleportation`` under the same models (#1194);
- ``--entropy-corrected`` and ``--preferred-number-of-modules`` under any
  objective but the ordinary map equation: state, multilayer and meta-data
  input (#904).

A combination the run refuses with an input error cannot be combined with
anything else, so its pair is left out of the array and checked on its own
(``test_refused_pair``):

- ``precomputed`` flow on multilayer input, which has no field for state flow;
- ``--regularized`` on multilayer input, which needs a build with
  ``FEATURES=regularized-multilayer`` and is refused for ``*Multilayer`` input
  in any build.

Some are refused per network rather than per pair:

- precomputed flow on a file without node weights; node weights that do not
  sum to 1 are normalised with a warning;
- directed flow with recorded teleportation to links on bipartite input whose
  links all go from the primary to the feature side, which leaves no flow
  ("Degenerate flow").

A power iteration that does not converge is warned about and tolerated: the
regularized directed prior mixes slowly on a network of two nearly closed
groups, like the generated state network.

Known issues
------------
Where a known issue breaks an oracle for a whole class of runs, that check is
left out for that class and the issue is named in ``KNOWN``. The minimised
repros of new findings are pinned as strict xfails at the end of the file.
"""

from __future__ import annotations

import itertools
import logging
import math
import random
import re
from collections.abc import Callable, Mapping
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
import pytest
from infomap import Infomap
from infomap.errors import InfomapError
from reference_codelength import Leaf, Partition, codelength, total_degree
from reference_flow import MODELS, bipartite_projection, flow

pytestmark = pytest.mark.fast

REPO = Path(__file__).resolve().parents[2]
EXAMPLES = REPO / "examples" / "networks"
NETWORKS = REPO / "test" / "fixtures" / "networks"
META = REPO / "test" / "fixtures" / "meta"

REL = 1e-10
ABS = 1e-12

FACTORS: dict[str, list] = {
    "flow_model": [
        "undirected",
        "directed",
        "undirdir",
        "outdirdir",
        "rawdir",
        "precomputed",
    ],
    "network": ["plain", "bipartite", "multilayer", "state", "meta"],
    "markov_time": [1.0, 0.5, 2.0, "variable"],
    "search": ["N1", "N3", "N3-parallel"],
    "cluster_data": ["none", "soft", "hard"],
    "teleport_to": ["links", "nodes"],
    "recorded": [False, True],
    "alpha": [0.15, 0.3],
    "levels": ["multilevel", "two-level"],
    "preferred": [0, 2],
    "entropy": [False, True],
    "regularized": [False, True],
    "node_weights": [False, True],
    "threads": [1, 2],
}

# Pairs the run refuses with an input error, whatever else is set, as
# pair -> (error, index of the network in networks() to show it on).
REFUSED_PAIRS = {
    (("flow_model", "precomputed"), ("network", "multilayer")): (
        "Missing node flow",
        0,
    ),
    # Needs FEATURES=regularized-multilayer for *Intra/*Inter input, so it is shown
    # on *Multilayer input, which is refused in any build.
    (("network", "multilayer"), ("regularized", True)): (
        "Regularized multilayer flow requires",
        1,
    ),
}


def allowed(row: Mapping[str, object]) -> bool:
    """Whether a (partial) row holds none of the refused pairs."""
    for a, b in REFUSED_PAIRS:
        if row.get(a[0]) == a[1] and row.get(b[0]) == b[1]:
            return False
    return True


# --- the covering array ------------------------------------------------------


def covering_array(
    factors: Mapping[str, list], allowed: Callable[[Mapping[str, object]], bool]
) -> list[dict[str, object]]:
    """A small set of rows that holds every allowed pair of levels at least once.

    Greedy and deterministic: each row starts from the first pair not yet
    covered, and every other factor takes the level that covers the most new
    pairs with the factors already set (the first such level on a tie). Factors
    with more levels are set first, as in AETG.
    """
    names = list(factors)

    def named(row: Mapping[int, int]) -> dict[str, object]:
        return {names[k]: factors[names[k]][v] for k, v in row.items()}

    uncovered = {
        (i, a, j, b)
        for i, j in itertools.combinations(range(len(names)), 2)
        for a in range(len(factors[names[i]]))
        for b in range(len(factors[names[j]]))
        if allowed(named({i: a, j: b}))
    }
    order = sorted(range(len(names)), key=lambda k: -len(factors[names[k]]))
    rows = []
    while uncovered:
        i, a, j, b = min(uncovered)
        row = {i: a, j: b}
        for k in order:
            if k in row:
                continue
            best, best_gain = None, -1
            for c in range(len(factors[names[k]])):
                if not allowed(named({**row, k: c})):
                    continue
                gain = sum(
                    (min(k, m), c if k < m else v, max(k, m), v if k < m else c)
                    in uncovered
                    for m, v in row.items()
                )
                if gain > best_gain:
                    best, best_gain = c, gain
            assert best is not None, f"no allowed level for {names[k]} in {named(row)}"
            row[k] = best
        uncovered -= {
            (p, row[p], q, row[q]) for p, q in itertools.combinations(sorted(row), 2)
        }
        rows.append({name: factors[name][row[k]] for k, name in enumerate(names)})
    return rows


ROWS = covering_array(FACTORS, allowed)


def _shuffled(seed: int) -> dict[str, list]:
    # The slow sweep also runs arrays built with the levels of each factor in a
    # shuffled order. Each puts every pair in other company, so a bug that needs
    # a third option to show has more chances to meet it.
    rng = random.Random(seed)
    return {name: rng.sample(levels, len(levels)) for name, levels in FACTORS.items()}


SWEEP_ARRAYS = [ROWS] + [
    covering_array(_shuffled(seed), allowed) for seed in range(1, 10)
]
SWEEP = [
    (array, index)
    for array, rows in enumerate(SWEEP_ARRAYS)
    for index in range(len(rows))
]


def _row_id(index: int, array: int = 0) -> str:
    row = SWEEP_ARRAYS[array][index]
    return f"{array}.{index:02d}-{row['network']}-{row['flow_model']}"


# --- networks ----------------------------------------------------------------


@dataclass
class Net:
    """A network file with what the oracles need to know about it."""

    name: str
    kind: str
    path: Path
    meta: Path | None = None
    # Node weights from *Vertices (first-order) or *States (state networks).
    weights: dict[int, float] | None = None
    features: set[int] = field(default_factory=set)
    # Multilayer *Intra/*Inter input, which the engine runs as directed.
    directed_input: bool = False


def _write_pajek(path: Path, *, vertices, sections) -> Path:
    lines = [f"*Vertices {len(vertices)}"]
    for node, weight in vertices.items():
        lines.append(f'{node} "n{node}"' + ("" if weight is None else f" {weight:g}"))
    for header, rows in sections:
        lines.append(header)
        lines.extend(
            " ".join(f"{x:g}" if isinstance(x, float) else str(x) for x in r)
            for r in rows
        )
    path.write_text("\n".join(lines) + "\n")
    return path


def _generated_plain(rng, n=12, groups=3):
    group = np.arange(n) % groups
    links = []
    for u in range(n):
        for v in range(u + 1, n):
            p = 0.6 if group[u] == group[v] else 0.08
            if rng.random() < p:
                links.append((u + 1, v + 1, float(np.round(rng.uniform(0.5, 2.0), 3))))
    links.append((2, 2, 1.0))
    weights = {u + 1: float(np.round(rng.uniform(0.5, 2.0), 3)) for u in range(n)}
    return weights, links


def build_networks(out: Path) -> dict[str, list[Net]]:
    """The networks of each kind, generated ones written to ``out``.

    The first of each kind is the one the default run uses.
    """
    rng = np.random.default_rng(1171)

    weights, links = _generated_plain(rng)
    plain = _write_pajek(
        out / "plain.net", vertices=weights, sections=[("*Edges", links)]
    )

    # Nine primary nodes in three groups, each group sharing two of six features.
    primary = range(1, 10)
    feature_ids = range(10, 16)
    bip_links = []
    for node in primary:
        group = (node - 1) // 3
        for feature in (10 + 2 * group, 11 + 2 * group):
            if rng.random() < 0.8:
                bip_links.append(
                    (node, feature, float(np.round(rng.uniform(0.5, 2), 3)))
                )
        if rng.random() < 0.3:
            bip_links.append((node, int(rng.choice(feature_ids)), 0.5))
    bip_weights = {
        n: float(np.round(rng.uniform(0.5, 2), 3)) for n in [*primary, *feature_ids]
    }
    bipartite = _write_pajek(
        out / "bipartite.net",
        vertices=bip_weights,
        sections=[("*Bipartite 10", bip_links)],
    )

    # Two layers over eight physical nodes, intra links in two groups, and inter
    # links between the copies of four nodes.
    intra = []
    for layer in (1, 2):
        for u in range(1, 9):
            for v in range(1, 9):
                p = 0.5 if (u - 1) // 4 == (v - 1) // 4 else 0.1
                if u != v and rng.random() < p:
                    intra.append((layer, u, v, float(np.round(rng.uniform(0.5, 2), 3))))
    inter = [(1, n, 2, 0.5) for n in (1, 3, 5, 7)] + [(2, n, 1, 0.5) for n in (2, 6)]
    multilayer = _write_pajek(
        out / "multilayer.net",
        vertices=dict.fromkeys(range(1, 9)),
        sections=[("*Intra", intra), ("*Inter", inter)],
    )

    # A state network over eight physical nodes: nodes 1-4 have two states each.
    states = [(s, s if s <= 8 else s - 8) for s in range(1, 13)]
    state_weights = {s: float(np.round(rng.uniform(0.5, 2), 3)) for s, _ in states}
    state_links = []
    for s in range(1, 13):
        for t in range(1, 13):
            if s != t and rng.random() < (0.45 if (s % 2) == (t % 2) else 0.08):
                state_links.append((s, t, float(np.round(rng.uniform(0.5, 2), 3))))
    state = _write_pajek(
        out / "states.net",
        vertices=dict.fromkeys(range(1, 9)),
        sections=[
            ("*States", [(s, n, f'"s{s}"', state_weights[s]) for s, n in states]),
            ("*Links", state_links),
        ],
    )

    meta_weights, meta_links = _generated_plain(rng)
    meta_net = _write_pajek(
        out / "meta.net", vertices=meta_weights, sections=[("*Edges", meta_links)]
    )
    meta_file = out / "meta.meta"
    meta_file.write_text("".join(f"{n} {(n * 7) % 4 + 1}\n" for n in meta_weights))

    def vertex_weights(path: Path) -> dict[int, float] | None:
        weights, section = {}, None
        for line in path.read_text().splitlines():
            line = line.split("#", 1)[0].strip()
            if line.startswith("*"):
                section = line.split()[0].lower()
                continue
            if section == "*vertices" and line:
                rest = line.split('"')[-1].split()
                if rest:
                    weights[int(line.split()[0])] = float(rest[0])
        return weights or None

    def fixture(name, kind, path, **kw):
        return Net(
            name, kind, path, weights=kw.pop("weights", vertex_weights(path)), **kw
        )

    return {
        "plain": [
            Net("generated", "plain", plain, weights=weights),
            fixture("twotriangles_flow", "plain", NETWORKS / "twotriangles_flow.net"),
            fixture("ninetriangles", "plain", EXAMPLES / "ninetriangles.net"),
        ],
        "bipartite": [
            Net(
                "generated",
                "bipartite",
                bipartite,
                weights=bip_weights,
                features=set(feature_ids),
            ),
            fixture(
                "bipartite", "bipartite", EXAMPLES / "bipartite.net", features={4, 5}
            ),
            fixture(
                "bipartite_uneven",
                "bipartite",
                NETWORKS / "bipartite_uneven.net",
                features={4, 5, 6, 7},
            ),
        ],
        "multilayer": [
            # *Intra/*Inter links are directed input; *Multilayer links are not.
            Net("generated", "multilayer", multilayer, directed_input=True),
            Net("multilayer", "multilayer", EXAMPLES / "multilayer.net"),
            Net(
                "multilayer_intra_inter",
                "multilayer",
                EXAMPLES / "multilayer_intra_inter.net",
                directed_input=True,
            ),
        ],
        "state": [
            Net("generated", "state", state, weights=state_weights),
            Net(
                "states_flow",
                "state",
                NETWORKS / "states_flow.net",
                weights={1: 0.1, 2: 0.1, 3: 0.3, 4: 0.3, 5: 0.1, 6: 0.1},
            ),
            Net("states", "state", EXAMPLES / "states.net"),
        ],
        "meta": [
            Net("generated", "meta", meta_net, meta=meta_file, weights=meta_weights),
            fixture(
                "twotriangles_flow",
                "meta",
                NETWORKS / "twotriangles_flow.net",
                meta=META / "twotriangles.meta",
            ),
            fixture(
                "ninetriangles",
                "meta",
                EXAMPLES / "ninetriangles.net",
                meta=META / "ninetriangles_crossing.meta",
            ),
        ],
    }


# --- running a case ----------------------------------------------------------


@dataclass
class Expected:
    """What a row means for a network: the run it is, after the options it ignores."""

    flow_model: str
    error: str | None = None
    warnings: set[str] = field(default_factory=set)
    regularized: bool = False
    recorded: bool = False
    memory: bool = False
    biased: bool = True


WARN_REGULARIZED = "--regularized has no effect"
WARN_RECORDED = "--recorded-teleportation has no effect"
WARN_ENTROPY = "--entropy-corrected has no effect"
WARN_PREFERRED = "--preferred-number-of-modules has no effect"
WARN_NORMALISED = "Node flow sums to"
# Warned about, but not an option the run ignores; see the module docstring.
TOLERATED = ("PageRank calculation did not converge",)


def expected(row: Mapping[str, object], net: Net) -> Expected:
    model = str(row["flow_model"])
    if model in ("undirected", "undirdir") and net.directed_input:
        # Multilayer *Intra/*Inter input expands its links to both directions and
        # runs directed (Network::generateStateNetworkFromMultilayer), undirdir
        # included, with a note at verbosity 1 only.
        model = "directed"
    multilayer = net.kind == "multilayer"
    exp = Expected(flow_model=model, memory=net.kind in ("state", "multilayer"))
    exp.biased = net.kind not in ("state", "multilayer", "meta")
    exp.regularized = bool(row["regularized"]) and (
        model in ("directed", "undirected") or multilayer
    )
    # --regularized turns recorded teleportation on itself.
    asked_recorded = bool(row["recorded"]) or bool(row["regularized"])
    exp.recorded = asked_recorded and (model == "directed" or exp.regularized)
    if row["regularized"] and not exp.regularized:
        exp.warnings.add(WARN_REGULARIZED)
    elif asked_recorded and not exp.recorded and model != "undirected":
        exp.warnings.add(WARN_RECORDED)
    if not exp.biased and row["entropy"]:
        exp.warnings.add(WARN_ENTROPY)
    if not exp.biased and row["preferred"]:
        exp.warnings.add(WARN_PREFERRED)
    if (
        net.kind == "bipartite"
        and model == "directed"
        and exp.recorded
        and not exp.regularized
        and row["teleport_to"] == "links"
        and not row["node_weights"]
    ):
        exp.error = "Degenerate flow"
    if model == "precomputed":
        if not net.weights:
            exp.error = "Missing node flow"
        elif not math.isclose(sum(net.weights.values()), 1.0):
            exp.warnings.add(WARN_NORMALISED)
    return exp


def options(row: Mapping[str, object], net: Net, seed: int) -> dict[str, object]:
    opts: dict[str, object] = {
        "silent": True,
        "seed": seed,
        "flow_model": row["flow_model"],
        "teleportation_probability": row["alpha"],
        "num_threads": row["threads"],
    }
    if row["markov_time"] == "variable":
        opts["variable_markov_time"] = True
    elif row["markov_time"] != 1.0:
        opts["markov_time"] = row["markov_time"]
    opts["num_trials"] = 1 if row["search"] == "N1" else 3
    if row["search"] == "N3-parallel":
        opts["parallel_trials"] = True
    if row["teleport_to"] == "nodes":
        opts["to_nodes"] = True
    if row["recorded"]:
        opts["recorded_teleportation"] = True
    if row["levels"] == "two-level":
        opts["two_level"] = True
    if row["preferred"]:
        opts["preferred_number_of_modules"] = row["preferred"]
    if row["entropy"]:
        opts["entropy_corrected"] = True
    if row["regularized"]:
        opts["regularized"] = True
    if row["node_weights"]:
        opts["use_node_weights_as_flow"] = True
    if net.meta is not None:
        opts["meta_data"] = str(net.meta)
    return opts


class _Messages(logging.Handler):
    def __init__(self) -> None:
        super().__init__(level=logging.DEBUG)
        self.lines: list[str] = []

    def emit(self, record: logging.LogRecord) -> None:
        self.lines.append(record.getMessage())


def _run(net: Net, opts: Mapping[str, object], *, hard: bool = False):
    """Run with every option set on the engine, so a hard partition survives the run."""
    handler = _Messages()
    logger = logging.getLogger("infomap")
    old_level = logger.level
    logger.addHandler(handler)
    logger.setLevel(logging.DEBUG)
    try:
        im = Infomap(**opts)
        im.read_file(str(net.path))
        if hard:
            # No CLI flag or keyword; the engine re-parses its options when a run
            # passes new ones, so it is set here and the run passes none.
            im._core._im.clusterDataIsHard = True
        result = im.run()
    finally:
        logger.removeHandler(handler)
        logger.setLevel(old_level)
    return result, handler.lines


def _warnings(lines: list[str]) -> list[str]:
    return [line.strip() for line in lines if line.strip().startswith("Warning:")]


def _logged_model(lines: list[str]) -> str | None:
    for line in lines:
        match = re.match(r"\s*Model\s+(\S+)", line)
        if match:
            return match.group(1)
    return None


def _write_seed(net: Net, path: Path, base_opts: Mapping[str, object]) -> Path:
    """A partition into three modules by leaf order, for --cluster-data."""
    result, _ = _run(net, {**base_opts, "no_infomap": True})
    leaves = sorted(
        (node.state_id, node.node_id, node.layer_id)
        for node in result.tree(states=True)
        if node.is_leaf
    )
    with path.open("w") as fh:
        for i, (state, node, layer) in enumerate(leaves):
            # state_id module flow node_id layer_id; the last three are read for multilayer input only.
            fh.write(f"{state} {i % 3 + 1} 0 {node} {layer}\n")
    return path


@dataclass
class Case:
    row: dict[str, object]
    net: Net
    seed: int

    @property
    def label(self) -> str:
        return f"{self.net.kind}/{self.net.name} seed {self.seed}"


@dataclass
class Outcome:
    case: Case
    exp: Expected
    error: str | None = None
    warnings: list[str] = field(default_factory=list)
    # check name -> what went wrong
    failures: dict[str, str] = field(default_factory=dict)
    checked: set[str] = field(default_factory=set)


def _partition(result, case: Case, exp: Expected) -> Partition:
    row, net = case.row, case.net
    meta = {}
    if net.meta is not None:
        for line in net.meta.read_text().splitlines():
            parts = line.split("#", 1)[0].split()
            if len(parts) >= 2:
                meta[int(parts[0])] = int(parts[1])
    leaves = {}
    for node in result.tree(states=True):
        if not node.is_leaf:
            continue
        leaves[node.state_id] = Leaf(
            path=tuple(node.path),
            flow=node.flow,
            physical_id=node.node_id,
            meta=meta.get(node.node_id, -1),
            teleport_flow=node.data.teleportFlow if exp.recorded else 0.0,
            teleport_weight=node.data.teleportWeight if exp.recorded else 0.0,
        )
    undirected = exp.flow_model == "undirected"
    markov = row["markov_time"]
    return Partition(
        leaves=leaves,
        links=list(result.links("flow")),
        undirected_links=undirected,
        markov_time=1.0 if markov == "variable" else float(markov),
        memory=exp.memory and net.meta is None,
        recorded_teleportation=exp.recorded,
        meta_rate=1.0 if net.meta is not None else None,
        preferred_num_modules=int(row["preferred"]) if exp.biased else 0,
        entropy_correction_strength=1.0 if exp.biased and row["entropy"] else None,
        total_degree=total_degree(result.links("weight"), undirected=undirected),
    )


def _close(a: float, b: float) -> bool:
    return math.isclose(a, b, rel_tol=REL, abs_tol=ABS)


def _check_flow(result, case: Case, exp: Expected) -> str | None:
    row, net = case.row, case.net
    leaves = {n.state_id: n for n in result.tree(states=True) if n.is_leaf}
    weights = result.links("weight")
    if net.kind == "bipartite":
        full = flow(list(leaves), weights, "undirected")
        reference = bipartite_projection(full, net.features)
    else:
        reference = flow(
            list(leaves),
            weights,
            exp.flow_model,
            teleportation_probability=float(row["alpha"]),
            recorded=exp.recorded,
            to_nodes=row["teleport_to"] == "nodes",
            node_weights=net.weights,
        )
    for node_id, value in reference.node.items():
        if not _close(leaves[node_id].flow, value):
            return (
                f"node {node_id} flow {leaves[node_id].flow:.12g}, oracle {value:.12g}"
            )
    links = {(s, t): v for s, t, v in result.links("flow")}
    for link, value in reference.link.items():
        if not _close(links[link], value):
            return f"link {link} flow {links[link]:.12g}, oracle {value:.12g}"
    return None


def evaluate(case: Case, tmp: Path) -> Outcome:
    """Run one case and every oracle that covers it; collect what disagrees."""
    row, net = case.row, case.net
    exp = expected(row, net)
    outcome = Outcome(case, exp)
    opts = options(row, net, case.seed)
    if row["cluster_data"] != "none":
        base = {
            k: v for k, v in opts.items() if k in ("silent", "flow_model", "meta_data")
        }
        if exp.error is None:
            opts["cluster_data"] = str(
                _write_seed(net, tmp / f"{net.kind}-{net.name}.clu", base)
            )
    try:
        result, lines = _run(net, opts, hard=row["cluster_data"] == "hard")
    except InfomapError as error:
        outcome.error = str(error)
        if exp.error is None or exp.error not in outcome.error:
            outcome.failures["runs"] = outcome.error
        return outcome
    outcome.warnings = [
        line for line in _warnings(lines) if not any(t in line for t in TOLERATED)
    ]
    if exp.error is not None:
        outcome.failures["runs"] = f"expected the error {exp.error!r}, the run finished"
        return outcome

    noted = {w for w in exp.warnings if any(w in line for line in outcome.warnings)}
    unexpected = [
        line for line in outcome.warnings if not any(w in line for w in exp.warnings)
    ]
    if noted != exp.warnings or unexpected:
        outcome.failures["warnings"] = (
            f"missing {sorted(exp.warnings - noted)}, unexpected {unexpected}"
        )
    model = _logged_model(lines)
    if model != exp.flow_model:
        outcome.failures["flow model"] = f"ran {model}, expected {exp.flow_model}"

    # The reference codelength.
    if covers_codelength(case, exp):
        outcome.checked.add("codelength")
        reference = codelength(_partition(result, case, exp))
        if not _close(result.codelength, reference.total):
            outcome.failures["codelength"] = (
                f"reported {result.codelength:.12g}, reference {reference.total:.12g}"
            )

    # The returned tree, read back and scored with --no-infomap.
    outcome.checked.add("read back")
    tree = tmp / f"{net.kind}-{net.name}-{case.seed}.tree"
    result.write_tree(str(tree), states=exp.memory)
    back_opts = {
        k: v
        for k, v in opts.items()
        if k not in ("cluster_data", "num_trials", "parallel_trials", "seed")
    }
    try:
        back, _ = _run(
            net, {**back_opts, "cluster_data": str(tree), "no_infomap": True}
        )
    except InfomapError as error:
        outcome.failures["read back"] = f"refused: {error}"
    else:
        if not _close(result.codelength, back.codelength):
            outcome.failures["read back"] = (
                f"reported {result.codelength:.12g}, read back {back.codelength:.12g}"
            )

    # The flow oracle.
    if covers_flow(case, exp):
        outcome.checked.add("flow")
        problem = _check_flow(result, case, exp)
        if problem:
            outcome.failures["flow"] = problem
    return outcome


def covers_codelength(case: Case, exp: Expected) -> bool:
    """Whether the reference codelength covers the objective of this run."""
    # Only a global Markov time, and not the per-layer teleportation terms of
    # regularized multilayer flow.
    return case.row["markov_time"] != "variable" and not (
        exp.regularized and case.net.kind == "multilayer"
    )


def covers_flow(case: Case, exp: Expected) -> bool:
    """Whether the flow oracle covers the flow model of this run."""
    if exp.regularized or case.row["node_weights"]:
        return False
    if case.net.kind == "bipartite":
        return exp.flow_model == "undirected"
    if case.net.kind == "multilayer":
        return False
    return exp.flow_model in MODELS


# --- known issues ------------------------------------------------------------


def _markov_above_one(o: Outcome) -> bool:
    t = o.case.row["markov_time"]
    return t == "variable" or float(t) > 1


def _markov_below_one(o: Outcome) -> bool:
    t = o.case.row["markov_time"]
    return t != "variable" and float(t) < 1


def _degenerate_with_node_weights(o: Outcome) -> bool:
    # The run that is refused as degenerate without --use-node-weights-as-flow.
    return (
        o.case.row["node_weights"]
        and expected({**o.case.row, "node_weights": False}, o.case.net).error
        == "Degenerate flow"
    )


def _negative_flow(o: Outcome) -> bool:
    # The release guard (#963) and the debug invariant word it the same way.
    return o.error is not None and "Negative" in o.error


def _unbalanced_flow(o: Outcome) -> bool:
    # Enter and exit flow that do not balance; caught by the debug invariant only.
    return o.error is not None and "do not balance" in o.error


# Open issues the sweep reaches, as check -> (issue, predicate). A failure of
# that check on a run the predicate matches is put down to the issue. Some of
# these depend on the search path, which differs between standard libraries
# (#342), so a predicate may match runs that pass here.
KNOWN: dict[str, list[tuple[str, Callable[[Outcome], bool]]]] = {
    "runs": [
        # --regularized with Markov time above 1, or variable Markov time.
        (
            "#1191",
            lambda o: o.exp.regularized and _markov_above_one(o) and _negative_flow(o),
        ),
        # --regularized with --use-node-weights-as-flow.
        (
            "#1191",
            lambda o: (
                o.exp.regularized
                and bool(o.case.row["node_weights"])
                and _negative_flow(o)
            ),
        ),
        # --variable-markov-time with --use-node-weights-as-flow.
        (
            "#1191",
            lambda o: (
                o.case.row["markov_time"] == "variable"
                and bool(o.case.row["node_weights"])
                and _negative_flow(o)
            ),
        ),
        # Bipartite input with recorded teleportation.
        (
            "#1175",
            lambda o: (
                o.case.net.kind == "bipartite"
                and o.exp.recorded
                and (_negative_flow(o) or _unbalanced_flow(o))
            ),
        ),
        # Not filed yet: node weights as flow get past the degenerate-flow refusal (see below).
        (
            "bipartite-node-weights-degenerate",
            lambda o: _degenerate_with_node_weights(o) and o.error is not None,
        ),
    ],
    "codelength": [
        ("#1175", lambda o: o.case.net.kind == "bipartite" and o.exp.recorded),
        ("bipartite-node-weights-degenerate", _degenerate_with_node_weights),
        # Not filed yet: --regularized with Markov time below 1 (see the pinned test below).
        (
            "regularized-markov-time-below-1",
            lambda o: o.exp.regularized and _markov_below_one(o),
        ),
        ("#1189", lambda o: o.exp.recorded and bool(o.case.row["node_weights"])),
        ("#1207", lambda o: o.case.row["cluster_data"] == "hard"),
        (
            "#1209",
            lambda o: (
                o.case.net.kind == "state" and o.case.row["cluster_data"] != "none"
            ),
        ),
    ],
    "read back": [
        ("#1175", lambda o: o.case.net.kind == "bipartite" and o.exp.recorded),
        (
            "regularized-markov-time-below-1",
            lambda o: o.exp.regularized and _markov_below_one(o),
        ),
        (
            "#1191",
            lambda o: (
                o.exp.regularized
                and _markov_above_one(o)
                and "Negative" in o.failures["read back"]
            ),
        ),
        ("#1189", lambda o: o.exp.recorded and bool(o.case.row["node_weights"])),
        ("#1207", lambda o: o.case.row["cluster_data"] == "hard"),
        (
            "#1209",
            lambda o: (
                o.case.net.kind == "state" and o.case.row["cluster_data"] != "none"
            ),
        ),
    ],
}


def known(outcome: Outcome, check: str) -> str | None:
    for issue, applies in KNOWN.get(check, []):
        if applies(outcome):
            return issue
    return None


def assert_case(case: Case, tmp: Path) -> None:
    outcome = evaluate(case, tmp)
    unexplained = {
        check: what
        for check, what in outcome.failures.items()
        if known(outcome, check) is None
    }
    assert not unexplained, f"{case.label} {case.row}: {unexplained}"


# --- tests -------------------------------------------------------------------


@pytest.fixture(scope="module")
def networks(tmp_path_factory) -> dict[str, list[Net]]:
    return build_networks(tmp_path_factory.mktemp("pairwise"))


def test_covering_array_holds_every_allowed_pair():
    names = list(FACTORS)
    for x, y in itertools.combinations(names, 2):
        for a in FACTORS[x]:
            for b in FACTORS[y]:
                if allowed({x: a, y: b}):
                    assert any(r[x] == a and r[y] == b for r in ROWS), (x, a, y, b)
    assert all(allowed(row) for row in ROWS)


@pytest.mark.parametrize(
    ("pair", "refusal"), REFUSED_PAIRS.items(), ids=lambda p: "-".join(map(str, p))
)
def test_refused_pair(pair, refusal, networks):
    (fa, a), (fb, b) = pair
    message, network = refusal
    row = {name: levels[0] for name, levels in FACTORS.items()} | {fa: a, fb: b}
    net = networks[row["network"]][network]
    with pytest.raises(InfomapError, match=message):
        _run(net, options(row, net, 1))


@pytest.mark.parametrize("index", range(len(ROWS)), ids=_row_id)
def test_pairwise_row(index, networks, tmp_path):
    row = ROWS[index]
    assert_case(Case(row, networks[row["network"]][0], seed=1), tmp_path)


@pytest.mark.slow
@pytest.mark.parametrize("network", range(3), ids=["n0", "n1", "n2"])
@pytest.mark.parametrize(
    ("array", "index"), SWEEP, ids=[_row_id(i, a) for a, i in SWEEP]
)
def test_pairwise_sweep(array, index, network, networks, tmp_path):
    row = SWEEP_ARRAYS[array][index]
    for seed in (1, 2):
        assert_case(Case(row, networks[row["network"]][network], seed), tmp_path)


# --- findings ----------------------------------------------------------------


@pytest.mark.xfail(
    reason="--regularized with --markov-time below 1 takes leaf enter and exit flow from the "
    "node flow, which Markov time does not scale, and subtracts the scaled internal link flow "
    "when it builds a module, so a module keeps exit flow it does not have: one module over the "
    "whole network costs more than the one-level codelength. The twin of #1191 (above 1, "
    "negative flow); not filed yet",
    strict=True,
)
@pytest.mark.parametrize("directed", [False, True], ids=["undirected", "directed"])
@pytest.mark.parametrize("markov_time", [0.5, 0.8])
def test_regularized_markov_time_below_one(markov_time, directed):
    result, _ = _run(
        Net("twotriangles", "plain", EXAMPLES / "twotriangles.net"),
        {
            "silent": True,
            "regularized": True,
            "markov_time": markov_time,
            "directed": directed,
            "cluster_data": str(
                REPO
                / "test"
                / "fixtures"
                / "clusters"
                / "twotriangles_single_module.clu"
            ),
            "no_infomap": True,
        },
    )
    assert result.num_top_modules == 1
    assert result.codelength == pytest.approx(
        result.one_level_codelength, rel=REL, abs=ABS
    )


def _leaf_flow_balance(result) -> tuple[float, float]:
    enter = exit_ = 0.0
    for node in result.tree():
        if node.is_leaf:
            enter += node.data.enterFlow
            exit_ += node.data.exitFlow
    return enter, exit_


@pytest.mark.xfail(
    reason="directed flow with recorded teleportation to links leaves bipartite input without "
    "flow, and the run is refused as degenerate, but --use-node-weights-as-flow gets past that "
    "check: the feature nodes take every teleport weight and are not coded (#1175), so the "
    "primary nodes exit by teleportation and are never entered. Not filed yet",
    strict=True,
)
@pytest.mark.parametrize(
    "network",
    [EXAMPLES / "bipartite.net", NETWORKS / "bipartite_uneven.net"],
    ids=lambda p: p.stem,
)
def test_bipartite_recorded_teleportation_with_node_weights_as_flow(network):
    opts = {
        "silent": True,
        "num_trials": 1,
        "directed": True,
        "recorded_teleportation": True,
        "use_node_weights_as_flow": True,
        "two_level": True,
    }
    # Without the option the run is refused, which is what the test is about.
    with pytest.raises(InfomapError, match="Degenerate flow"):
        _run(
            Net(network.stem, "bipartite", network),
            {**opts, "use_node_weights_as_flow": False},
        )
    # A flow invariant violation in invariant builds; a wrong codelength in release.
    result, _ = _run(Net(network.stem, "bipartite", network), opts)
    enter, exit_ = _leaf_flow_balance(result)
    assert enter == pytest.approx(exit_, rel=REL, abs=ABS)
