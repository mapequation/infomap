---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Running Infomap

{bdg-success-line}`How-to`

```{admonition} At a glance
:class: tip
Most runs need only a `seed`, a `num_trials` count, and perhaps a flow-model
choice such as `directed`. The rest of Infomap's options split into flow-model
settings, which define the random walk, and search settings, which control how
hard Infomap looks.
```

Add `two_level` if you want a flat partition instead of the multilevel default.
The shortest useful run:

```python
import networkx as nx
import infomap

result = infomap.run(
    nx.karate_club_graph(),
    seed=123,        # reproducible
    num_trials=10,   # keep the best of 10 restarts
    two_level=True,  # flat partition; omit for the multilevel default
)
print(result.num_top_modules, result.codelength)
```

Everything below is when and why to reach for the other options. For the *why*
behind the flow model itself, see {doc}`/concepts/index`.

## Two kinds of options

Infomap has dozens of options, but they fall into two groups:

- **Flow-model options** (`directed`, `markov_time`, `teleportation_probability`)
  change *how the random walk is defined*. They encode your beliefs about the
  system; the wrong choice gives a partition that scores well but means little.
- **Search options** (`seed`, `num_trials`, `converge`) change *how hard the
  search works*, not which partition Infomap looks for. They only affect how
  reliably the stochastic search finds the optimum {cite:p}`calatayud2019solution`.

The theory behind them lives in Concepts:
{doc}`/concepts/flow-and-random-walks` for flow and teleportation,
{doc}`/concepts/hierarchy-and-the-multilevel-map` and
{doc}`/concepts/choosing-a-method` for resolution, and
{doc}`/robustness/incomplete-data` for regularization.

## The options, one task each

Each option below comes with a small graph you can run as you read.

### Setting up

```{code-cell} python
import networkx as nx
import infomap
import random

# ── Helper: build an 8-clique ring (clear, clean structure) ──────────────────
G_ring = nx.ring_of_cliques(8, 5)   # 8 cliques × 5 nodes = 40 nodes

# ── Helper: build a weakly-structured graph (noisy, degenerate landscape) ───
def make_weak_structure(n_communities=6, n_per=8, p_in=0.55, p_out=0.08, seed=123):
    """Planted-partition graph with intentionally weak community signal."""
    rng = random.Random(seed)
    G = nx.Graph()
    n = n_communities * n_per
    G.add_nodes_from(range(n))
    for i in range(n):
        for j in range(i + 1, n):
            ci, cj = i // n_per, j // n_per
            p = p_in if ci == cj else p_out
            if rng.random() < p:
                G.add_edge(i, j)
    return G

G_noisy = make_weak_structure()
print(f"Ring of cliques: {G_ring.number_of_nodes()} nodes, {G_ring.number_of_edges()} edges")
print(f"Weak structure:  {G_noisy.number_of_nodes()} nodes, {G_noisy.number_of_edges()} edges")
```

### `seed` and reproducibility

`seed` fixes the internal random number generator. With the same seed you get
the same partition on every run, which makes results shareable and publishable.
The default `seed=123` is reproducible too, but setting the seed yourself ties
each result to the code that produced it.

```{code-cell} python
# Two runs with the same seed produce the same partition.
for attempt in range(2):
    result = infomap.run(G_ring, two_level=True, seed=123, num_trials=10)
    print(f"Run {attempt + 1}: modules={result.num_top_modules}, L={result.codelength:.4f}")
```

### `num_trials` and solution quality

A single trial can get trapped in a local minimum. On graphs with clear,
distinct communities, like the ring of cliques, this rarely matters because most
starting points converge to the same solution. On a noisy or ambiguous graph the
landscape has many near-degenerate minima, and a single trial is unreliable.

```{code-cell} python
# On the noisy graph, different seeds land in different local minima.
print("Single trial (num_trials=1), five different seeds:")
for seed in [1, 7, 42, 99, 123]:
    result = infomap.run(G_noisy, two_level=True, seed=seed, num_trials=1)
    print(f"  seed={seed}: L={result.codelength:.4f}, modules={result.num_top_modules}")
```

```{code-cell} python
# More trials explore more of the landscape and find a lower, stable minimum.
print("Fixed seed=123, varying num_trials:")
for num_trials in [1, 5, 20]:
    result = infomap.run(G_noisy, two_level=True, seed=123, num_trials=num_trials)
    print(f"  num_trials={num_trials:2d}: L={result.codelength:.4f}, modules={result.num_top_modules}")
```

Here the single trial with `seed=123` lands at a higher codelength (worse
minimum) than `num_trials=20` with the same seed: a single trial can settle in a
local trap, and the extra trials find a partition that compresses the random
walk more efficiently. Which single trial gets stuck depends on the seed, so
the gap is not guaranteed for any particular seed. The codelength converges:
once you are reliably in the deepest valley, adding more trials does not change
the answer {cite:p}`calatayud2019solution`.

**Rule of thumb.** Use `num_trials=1` for quick exploration. Use
`num_trials=10` for most analyses. Use `num_trials=20` or more (up to around
`50`, or `converge=True` with a higher cap) when you need high confidence in
the global minimum, when you are comparing partitions across networks, or on
networks with weak or diffuse community structure.

```{admonition} converge=True
:class: note
`converge=True` treats `num_trials` as a cap and stops early once the best
codelength has stopped improving. This gives you "as many trials as you need
but no more," which is useful when you are unsure how many trials your network
requires.
```

### `two_level` versus multilevel

By default Infomap finds a hierarchical partition with as many levels as the
flow supports. Each level is a coarser view of the same structure.
`two_level=True` forces a single flat partition, which helps when you know your
network is not hierarchical, or when you need a simple cluster assignment for
downstream analysis.

```{code-cell} python
# Build a graph with genuine nested hierarchy:
# 2 top groups → 2 subgroups each → 2 cliques of 5 nodes
# Edge weights shrink at each scale, creating clear nesting.
def hierarchical_graph():
    G = nx.Graph()
    nid = 0
    top_groups = []
    for _ in range(2):
        subgroups = []
        for _ in range(2):
            cliques = []
            for _ in range(2):
                members = list(range(nid, nid + 5))
                nid += 5
                for i in range(len(members)):
                    for j in range(i + 1, len(members)):
                        G.add_edge(members[i], members[j], weight=100.0)
                cliques.append(members)
            G.add_edge(cliques[0][0], cliques[1][0], weight=10.0)
            subgroups.append(cliques)
        G.add_edge(subgroups[0][0][0], subgroups[1][0][0], weight=1.0)
        top_groups.append(subgroups)
    G.add_edge(top_groups[0][0][0][0], top_groups[1][0][0][0], weight=0.1)
    return G

G_hier = hierarchical_graph()

for two_level in [False, True]:
    result = infomap.run(G_hier, two_level=two_level, seed=123, num_trials=10)

    m_top = result.modules(depth=1)    # top-level groups
    m_leaf = result.modules(depth=-1)  # finest-level assignments

    print(f"two_level={two_level}:")
    print(f"  levels={result.num_levels}, top modules={result.num_top_modules}")
    print(f"  distinct groups at depth 1:      {len(set(m_top.values()))}")
    print(f"  distinct groups at finest level: {len(set(m_leaf.values()))}")
    print(f"  L={result.codelength:.4f}")
```

With `two_level=False`, the multilevel solution recovers the built-in nesting:
the two main branches at the top and the eight cliques at the finest level,
reached with `result.modules(depth=-1)` (the cell above prints the exact counts).
The codelength is lower because the deeper code captures the real hierarchy. With
`two_level=True`, the eight cliques become the top-level modules and the nested
structure is invisible.

Use `result.modules(depth=k)` to access any level of the hierarchy. The default
`result.modules()` returns level 1, the coarsest (top modules); pass `depth=-1`
for the finest (leaf) level.

### `directed` and the flow model

When your links represent directed flow (citations, hyperlinks, metabolic
reactions, transit routes), `directed=True` tells Infomap to use the
directionality to define the random walk. The walker follows links in their
stated direction, and unrecorded teleportation keeps the walk ergodic. Ignoring
direction on the same graph often misses the real community structure, because
it treats every link as symmetric.

```{code-cell} python
# A directed graph: three directed cliques connected by directed bridges.
# The directed structure groups nodes by which clique's flow they participate in.
G_dir = nx.DiGraph()
# Three directed cliques
for offset in [0, 4, 8]:
    nodes = list(range(offset, offset + 4))
    for u in nodes:
        for v in nodes:
            if u != v:
                G_dir.add_edge(u, v)
# Directed bridges forming an outer loop
G_dir.add_edge(3, 4)
G_dir.add_edge(7, 8)
G_dir.add_edge(11, 0)

print(f"Directed graph: {G_dir.number_of_nodes()} nodes, {G_dir.number_of_edges()} edges")

for directed in [False, True]:
    result = infomap.run(G_dir, directed=directed, two_level=True,
                         seed=123, num_trials=10)
    print(f"directed={directed}: modules={result.num_top_modules}, L={result.codelength:.4f}")
```

Both settings recover three modules here because the graph is strongly
clustered. On noisier real-world directed networks the difference is more
pronounced: the directed walk respects asymmetric flow and tends to find tighter
modules corresponding to true circulation patterns.

**Teleportation probability.** `teleportation_probability` ($\tau$) sets the
random-surfer teleportation rate; the default `0.15` is rarely worth changing.
See {doc}`/concepts/flow-and-random-walks` for why unrecorded teleportation makes
the partition robust to $\tau$.

### `markov_time` as a resolution dial

The map equation operates at the natural timescale of one random-walk step.
`markov_time` shifts this timescale: values below 1 encode the walk more
frequently (favouring more, smaller modules) and values above 1 encode it less
frequently (favouring fewer, larger modules). This is the principled way to
examine structure at a specific resolution when the data does not have a natural
scale {cite:p}`kheirkhahzadeh2016markov`.

```{code-cell} python
# The ring of cliques has 8 genuine cliques. markov_time merges them
# into larger clusters as it increases, giving a resolution sweep.
print("markov_time sweep on a ring of 8 cliques (5 nodes each):")
print(f"  graph: {G_ring.number_of_nodes()} nodes, {G_ring.number_of_edges()} edges")
print()

for mt in [0.5, 1.0, 2.0, 4.0, 8.0]:
    result = infomap.run(G_ring, two_level=True, markov_time=mt,
                         seed=123, num_trials=10)
    print(f"  markov_time={mt:.1f}: {result.num_top_modules} modules")
```

At `markov_time=1.0` (the default) Infomap recovers all 8 cliques. As the
Markov time grows, smaller modules become too expensive to maintain and adjacent
cliques merge. This gives you a natural scale sweep without changing the network.

The multilevel map equation is usually the better first choice {cite:p}`kawamoto2015resolution`: it finds scale automatically and has a less restrictive
resolution limit than the two-level version. Use `markov_time` when you
specifically want to study structure at a given scale, or when reviewers ask
"what happens at a coarser resolution?"

### `regularized` for sparse data

`regularized=True` enables the Bayesian regularized map equation for sparse or
incompletely sampled networks, and `regularization_strength` scales the prior
(higher merges more modules); see {doc}`/robustness/incomplete-data` for why
sparse data over-split and how the prior fixes it. On a graph as small as the
karate club the prior quickly overwhelms the data:

```{code-cell} python
# On the small karate club the regularization prior competes with the data:
# at half strength the three modules survive, and at the default strength
# the prior already outweighs the data and collapses them into one.
G_karate = nx.karate_club_graph()

for label, options in [
    ("baseline",                  infomap.Options()),
    ("regularized, strength=0.5", infomap.Options(regularized=True, regularization_strength=0.5)),
    ("regularized (default 1.0)", infomap.Options(regularized=True)),
]:
    result = infomap.run(G_karate, options=options, two_level=True, seed=123, num_trials=10)
    print(f"{label}: modules={result.num_top_modules}, L={result.codelength:.4f}")
```

The default strength already merges everything on a network this small. For a
worked example on a sparsely sampled planted partition, see
{doc}`/robustness/incomplete-data`.

### Why trials matter, visualised

A picture of the effect: run the noisy graph once per seed with a single trial,
and compare those codelengths against the lowest a longer search finds.

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

# Codelength from one trial per seed ...
single_L = []
for s in range(1, 13):
    single_L.append(
        infomap.run(G_noisy, two_level=True, seed=s, num_trials=1).codelength
    )

# ... versus the lowest codelength a longer search finds. Several seeds at 50
# trials each, so the reference is the best partition rather than one lucky (or
# unlucky) run; take the floor of everything shown so the line is a true minimum.
best_L = min(
    infomap.run(G_noisy, two_level=True, seed=s, num_trials=50).codelength
    for s in (1, 42, 123)
)
best_L = min([best_L, *single_L])

fig, ax = plt.subplots(figsize=(6, 3.2))
ax.scatter(range(1, len(single_L) + 1), single_L,
           color="0.45", zorder=3, label="single trial (num_trials=1)")
ax.axhline(best_L, color="tab:blue", lw=2, label=f"lowest codelength found = {best_L:.3f}")
ax.set_xlabel("seed")
ax.set_ylabel("codelength (bits/step)")
ax.legend(loc="best", fontsize=8)
fig.tight_layout()
glue("fig-running-and-options", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-running-and-options
Each grey point is one trial's codelength for a different seed, scattered widely.
A single trial rarely reaches the best partition Infomap can find (blue line) —
most settle in a worse local minimum. Running more trials and keeping the lowest
codelength is what gets you there; here only seed 7 finds it in one trial.
```

## Reusable configuration

When you run several networks with the same settings, capture them once as an
{class}`~infomap.Options` and pass the instance to each run. Keyword overrides on
the call still take precedence:

```{code-cell} python
from infomap import Options

options = Options(two_level=True, seed=123, num_trials=10, silent=True)
for name, graph in [("ring of cliques", G_ring), ("karate club", G_karate)]:
    result = infomap.run(graph, options=options)
    print(f"{name}: {result.num_top_modules} modules, L={result.codelength:.4f}")
```

## Pitfalls

- **`seed=0` raises.** Infomap requires `seed >= 1`; use any positive integer.
- **Codelengths are not comparable across `meta_data_rate` values.**
  `result.codelength` reports the combined objective, the topological term plus
  the weighted attribute term, so it rises with the rate even when the
  partition does not change (see {doc}`/flow-models/metadata`).
- **More trials never hurt correctness, only runtime.** If repeated runs disagree,
  raise `num_trials` (or pass `converge=True`) rather than trusting one fit.
- **Sparse or under-sampled data over-splits.** Reach for `regularized=True`
  (see {doc}`/robustness/incomplete-data`).

## API pointers

The options in this chapter are keyword arguments to {func}`infomap.run` and to
{class}`infomap.Infomap` ({meth}`infomap.Network.run` takes them bundled via
its `options=` argument instead). After a run, the
metrics live on the returned {class}`~infomap.Result`:

- **{attr}`infomap.Result.codelength`** is the map equation value
  $L(\mathsf{M}^*)$ for the best partition found across all trials.
- **{attr}`infomap.Result.num_top_modules`** counts the modules at the top
  level of the hierarchy.
- **{attr}`infomap.Result.num_levels`** is the depth of the module hierarchy.
- **{meth}`infomap.Result.modules`** returns a `{node_id: module_id}` dict;
  pass `depth=k` (1 = coarsest) to access intermediate levels.
- **{attr}`infomap.Result.one_level_codelength`** evaluates $L$ on a flat
  single-module partition, a natural baseline.
- **{attr}`infomap.Result.relative_codelength_savings`** is the fractional gain
  over that baseline, $(L_\text{one} - L^*) / L_\text{one}$.

These are the keyword arguments to {func}`infomap.run`:

| Option | Type | Default | What it controls |
|---|---|---|---|
| `seed` | int | 123 | RNG seed; must be ≥ 1 |
| `num_trials` | int | 1 | Independent restarts; keep the best |
| `converge` | bool | False | Stop early when codelength plateaus |
| `two_level` | bool | False | Force a flat (non-hierarchical) partition |
| `directed` | bool | None | Respect link direction |
| `teleportation_probability` | float | 0.15 | $\tau$ in the random-surfer model |
| `recorded_teleportation` | bool | False | Include teleportation in flow encoding |
| `markov_time` | float | 1.0 | Resolution scale |
| `regularized` | bool | False | Bayesian regularization for sparse data |
| `regularization_strength` | float | 1.0 | How strongly to regularize |
| `silent` | bool | True | Suppress console output (the Python API is quiet by default) |

For the full set of options as a searchable table, see the
{class}`~infomap.Options` reference in the {doc}`/api/index`.

## Routing the engine log through Python logging

The Python API is quiet by default. When you do want the engine's progress
log — in a notebook, application logs, or a file — one line opts in:

```{code-cell} python
import infomap

infomap.enable_log()

result = infomap.run([(1, 2), (1, 3), (2, 3), (3, 4), (4, 5), (4, 6), (5, 6)])

infomap.disable_log()
print(f"Found {result.num_top_modules} modules")
```

{func}`~infomap.enable_log` attaches a plain handler to the standard
`"infomap"` logger; that is all the opt-in is. For full control — formats,
files, your logging pipeline — skip the helper and configure the logger with
standard {mod}`logging`; any handler attached directly to it engages the same
routing:

```python
import logging

logging.getLogger("infomap").addHandler(my_handler)
logging.getLogger("infomap").setLevel(logging.INFO)
```

How the routing behaves:

- **Logging is the control.** With handlers attached, every engine run emits
  records — no `silent=False` needed, and `silent=` is ignored for emission.
  Tune volume with logger/handler levels (`logging.WARNING` silences the
  engine's INFO lines); remove the handlers to go back to classic stdout
  output gated by `silent=`.
- **Levels map naturally.** Default lines arrive at `INFO`. A logger enabled
  for `DEBUG` (`infomap.enable_log(logging.DEBUG)`) automatically raises the
  engine verbosity to `-vv`, so its detail lines arrive at `DEBUG`.
- **Only handlers attached directly to `logging.getLogger("infomap")` count.**
  `logging.basicConfig(...)` alone does not reroute the engine log — without
  an `"infomap"` handler, `silent=False` prints to stdout exactly as before.
- **Configure logging before creating a stateful engine.** The engine bakes
  its silence in at construction, so an {class}`~infomap.Infomap` created
  *before* logging was configured cannot emit records (its runs warn to
  explain why). The one-shot {func}`infomap.run` always respects the current
  logging configuration.
- **Routed output is plain lines.** The colors and the live progress line are
  terminal features; log records get one clean line each.
- **{class}`~infomap.Network` engines are silent for their whole lifetime**
  (see {meth}`Network.run <infomap.Network.run>`), so records come from the
  stateful {class}`~infomap.Infomap` and non-`Network` {func}`infomap.run`
  inputs. The `infomap` command-line interface is unaffected.
- **`silent` is fixed at construction for the stateful engine.** Because the
  engine bakes silence in when the {class}`~infomap.Infomap` is built, passing
  `silent=False` to {meth}`Infomap.run <infomap.Infomap.run>` on an instance
  constructed silent (the default) cannot re-enable the classic stdout log —
  construct with `silent=False`, or route the log through logging. The one-shot
  {func}`infomap.run` builds a fresh engine per call, so its `silent=` takes
  effect there.

## Going deeper

- The {doc}`options guide notebook </examples/options-guide>` lists every
  option in a searchable table, generated from the installed package so it
  matches your version. Start here for the exhaustive reference.
- The survey (§4) covers method selection and the flow-model justification
  {cite:p}`smiljanic2026survey`.
- The quantitative case for multiple trials {cite:p}`calatayud2019solution`.
- Why unrecorded teleportation produces robust directed partitions
  {cite:p}`lambiotte2012smart`.
- Why the map equation's resolution limit is less restrictive than modularity's
  {cite:p}`kawamoto2015resolution`.
- The Markov-time mechanism and its efficient resolution sweeps
  {cite:p}`kheirkhahzadeh2016markov`.
