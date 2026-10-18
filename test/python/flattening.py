"""A multilevel result never codes worse than its flattenings (#1257).

The oracles of #1171 are two-level. This check needs no optimum: it scores a
returned hierarchy against two two-level partitions made from it.

* **Top**: every leaf in its top module.
* **Leaf**: every leaf in its leaf module, the module right above it.

All three are scored by Infomap with ``--no-infomap --cluster-data`` under the
objective of the run, the flattenings with ``--two-level``. The hierarchy must
not code worse than either. Over the same seed, a multilevel run must also not
code worse than a ``--two-level`` run.

Why each holds
--------------
Multilevel search starts with ``partition()``, the two-level search that
``--two-level`` runs with the same seed, and each later step only lowers the
codelength:

* a super level is kept only if it shortens the index codebook it replaces, so
  it never codes worse than its own leaf modules;
* a module's sub-partition is kept only if it codes better than the module
  flat, so a module never codes worse flat-out (the top flattening);
* a module is dissolved into its parent when that codes better (#1074);
* if the recursion ends above the super-level tree, that tree is restored
  (#837).

Each accept test allows ``--minimum-codelength-improvement`` (1e-10), and the
scores are sums over the tree, so the check allows ``1e-9`` bits per bit of
codelength, the tolerance of the ``fuzz_cluster`` rescore check.

The top flattening is the only one of the three that the accept tests prove.
The leaf flattening is proved for the super level, and for recursive levels
only one module at a time by the dissolve pass. A leaf violation is a lead:
either a level priced wrong, or a search that kept a level it could not
improve on. Minimise it before calling it a bug.

Like for like
-------------
Two objectives price the tree rather than each codebook, and both are scored
the same way on all three trees:

``--preferred-number-of-modules``
    Charges ``|K - K_pref|`` once, on the number of top modules ``K``. The top
    flattening has the same ``K``, so the term cancels. The leaf flattening has
    more top modules and pays its own term; that is what the objective charges
    for that partition. Sub-searches charge the term on their own module count
    too, which is never negative, so their accept test is stricter than the
    tree's price, never looser.

``--entropy-corrected``
    Charges one free parameter per codeword in every codebook, intermediate
    module codebooks included (#1176), at the whole network's total degree. Sub
    and super searches inherit that degree and node count. Each tree pays for
    the codebooks it has.

Where it does not apply
-----------------------
``--preferred-number-of-levels`` biases the accept tests towards a depth
without pricing the depth in the codelength, so a level that codes worse can
be kept on purpose. ``--no-infomap`` runs no search. A cluster-data tree deeper
than two levels starts the search from that tree instead of ``partition()``,
so the seed comparison does not apply; the flattenings still do.

Use from the command line
-------------------------
For a run of the CLI, for example in the benchmark sweep (#1256)::

    python test/python/flattening.py --infomap ./Infomap network.net out/network.tree -- <flags>

``<flags>`` are the objective flags of the run: flow model, Markov time,
teleportation, ``--preferred-number-of-modules``, ``--entropy-corrected`` and so
on, without cluster data or output options. Pass the ``_states.tree`` for
state and multilayer input. It prints the three codelengths and exits 1 on a
violation. Only the standard library is used, so it runs next to any build.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from collections.abc import Callable, Hashable, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import TypeVar

K = TypeVar("K", bound=Hashable)

# Bits of slack per bit of codelength; see the module docstring.
TOLERANCE = 1e-9


def flatten(paths: Mapping[K, Sequence], level: int) -> dict[K, tuple[int, int]]:
    """Tree paths flattened to two levels, as ``{leaf: (module, index)}``.

    ``level=1`` keeps each leaf's top module, ``level=-1`` its leaf module.
    Modules are numbered in the order their first leaf comes.
    """
    if level not in (1, -1):
        raise ValueError(f"level must be 1 or -1, not {level}")
    modules: dict[tuple, int] = {}
    sizes: dict[int, int] = {}
    flat = {}
    for leaf, path in paths.items():
        key = tuple(path[:1] if level == 1 else path[:-1])
        module = modules.setdefault(key, len(modules) + 1)
        sizes[module] = sizes.get(module, 0) + 1
        flat[leaf] = (module, sizes[module])
    return flat


def flatten_tree(text: str, level: int) -> str:
    """A ``.tree`` file flattened to two levels, as :func:`flatten` does.

    Only the path column changes; comments and the other columns are kept.
    """
    lines = text.splitlines()
    paths = {}
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped and not stripped.startswith("#"):
            paths[i] = stripped.split(maxsplit=1)[0].split(":")
    for i, (module, index) in flatten(paths, level).items():
        rest = lines[i].strip().split(maxsplit=1)[1]
        lines[i] = f"{module}:{index} {rest}"
    return "\n".join(lines) + "\n"


@dataclass(frozen=True)
class Flattenings:
    """The codelength of a hierarchy and of its two flattenings."""

    hierarchy: float
    top: float
    leaf: float

    def violations(self) -> list[str]:
        slack = TOLERANCE * max(1.0, abs(self.hierarchy))
        found = []
        for name, flat in (("top", self.top), ("leaf", self.leaf)):
            if self.hierarchy > flat + slack:
                found.append(
                    f"the hierarchy codes {self.hierarchy:.12g} bits, "
                    f"{self.hierarchy - flat:.3g} more than its {name} flattening"
                )
        return found


def codes_no_worse(multilevel: float, two_level: float) -> bool:
    """Whether a multilevel codelength is within tolerance of a two-level one."""
    return multilevel <= two_level + TOLERANCE * max(1.0, abs(multilevel))


def flattenings(
    tree: Path, score: Callable[[Path, bool], float], work: Path | None = None
) -> Flattenings:
    """Score the hierarchy in ``tree`` and its two flattenings.

    ``score(tree_file, two_level)`` returns the codelength of the tree in
    ``tree_file`` under the objective of the run, scored with ``--no-infomap``
    and, if ``two_level``, ``--two-level``. The flattened files are written
    next to ``tree`` unless ``work`` is given.
    """
    work = tree.parent if work is None else work
    text = tree.read_text()
    flat = {}
    for name, level in (("top", 1), ("leaf", -1)):
        path = work / f"{tree.stem}.{name}.tree"
        path.write_text(flatten_tree(text, level))
        flat[name] = score(path, True)
    return Flattenings(score(tree, False), flat["top"], flat["leaf"])


# --- command line ------------------------------------------------------------


def cli_score(
    infomap: str, network: Path, flags: Sequence[str], work: Path
) -> Callable[[Path, bool], float]:
    """A ``score`` for :func:`flattenings` that runs the Infomap CLI."""

    def score(tree: Path, two_level: bool) -> float:
        summary = work / f"{tree.stem}.summary.json"
        args = [
            infomap,
            str(network),
            *flags,
            "--no-infomap",
            "--cluster-data",
            str(tree),
            "--silent",
            "--no-file-output",
            "--summary-json",
            str(summary),
        ]
        if two_level:
            args.append("--two-level")
        subprocess.run(args, check=True)
        return float(json.loads(summary.read_text())["codelength"])

    return score


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check that a tree never codes worse than its flattenings (#1257).",
        epilog="Put the run's objective flags after --.",
    )
    parser.add_argument("--infomap", default="./Infomap", help="the Infomap binary")
    parser.add_argument("network", type=Path)
    parser.add_argument(
        "tree", type=Path, help="the .tree (or _states.tree) of the run"
    )
    parser.add_argument("flags", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    flags = args.flags[1:] if args.flags[:1] == ["--"] else args.flags

    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        result = flattenings(
            args.tree, cli_score(args.infomap, args.network, flags, work), work
        )
    print(
        json.dumps(
            {
                "hierarchy": result.hierarchy,
                "top": result.top,
                "leaf": result.leaf,
                "violations": result.violations(),
            }
        )
    )
    return 1 if result.violations() else 0


if __name__ == "__main__":
    sys.exit(main())
