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

Most runs need only three choices: a `seed`, how many `num_trials` to run, and
either `two_level` or `directed` for the flow model. The shortest useful run:

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

## What matters

Infomap has dozens of options, but they fall into two groups, and telling them
apart is most of the battle:

- **Flow-model options** (`directed`, `markov_time`, `teleportation_probability`)
  change *how the random walk is defined*. They encode your beliefs about the
  system; the wrong choice gives a partition that scores well but means little.
- **Search options** (`seed`, `num_trials`, `converge`) change *how hard the
  search works*, not which partition Infomap looks for. They buy reliability.

The rest of this page is one task per option that matters. The flow-model theory
behind them lives in {doc}`/concepts/flow-and-random-walks` and
{doc}`/concepts/choosing-a-method`.

## How it works, in brief

Enough to use the options well; the full treatment is in
{doc}`/concepts/flow-and-random-walks` and {doc}`/concepts/choosing-a-method`.

### The search landscape: why `num_trials` matters

Finding the partition that minimises $L(\mathsf{M})$ is NP-hard, so Infomap
uses a stochastic greedy search. Each call to `run()` starts from a different
random initialisation (set by `seed`) and climbs downhill to a local minimum.
On a rugged landscape, different starting points often find different minima
{cite:p}`calatayud2019solution`. Infomap keeps the trial with the lowest codelength, so
more trials buy a more reliable partition at the cost of more compute.

`seed` fixes the random number generator for exact reproducibility. With the
same `seed` and `num_trials` on the same machine, you get the same partition
every time. With `seed` fixed and `num_trials=1`, a different `seed` value may
find a different minimum.

### Directed flow and teleportation

For undirected networks the random walk's stationary distribution is
proportional to degree, and no extra mechanism is needed. For directed networks
not every node is reachable, so Infomap uses a random-surfer model: with
probability $\tau$ the walker teleports to another node, and with probability
$1-\tau$ it follows a link. The default $\tau = 0.15$ is the conventional
PageRank value (one minus the 0.85 damping factor).

{cite:t}`lambiotte2012smart` showed that *what counts as a step* matters for
clustering (see {doc}`/concepts/flow-and-random-walks` for the full treatment of
teleportation). With *recorded* teleportation, the teleportation steps contribute to
the flow that defines module membership. With *unrecorded* teleportation (the
Infomap default), only steps along links count. This "smart teleportation" keeps
the partition stable as $\tau$ changes, because the artificial jumps no longer
blur module boundaries. Test it yourself: sweep `teleportation_probability` from
0.05 to 0.5 and watch unrecorded teleportation hold its partition across the
range while recorded teleportation drifts.

### Resolution scale: `markov_time`

The map equation balances description cost at the timescale of one random-walk
step. At that scale, a module is worth naming separately when the walker spends
longer inside it than outside. {cite:t}`kheirkhahzadeh2016markov` showed that rescaling
the transition rates by a factor $t$, the Markov time, shifts the resolution:
larger $t$ makes module crossings costlier and merges small modules into larger
ones, while smaller $t$ splits them.

The map equation has a far weaker resolution limit than modularity, and the
multilevel search loosens it further; {doc}`/concepts/hierarchy-and-the-multilevel-map`
and {doc}`/concepts/choosing-a-method` cover why. For most networks, let Infomap
find the multilevel hierarchy first, then reach for `markov_time` when you need a
specific scale.

### Regularization

Sparse or under-sampled networks can be over-partitioned: Infomap splits on
sampling noise instead of real structure. `regularized=True` adds a Bayesian
prior network that penalises splits unsupported by the data, merging weakly
supported modules. Scale the penalty with `regularization_strength` (default
1.0). This option is most useful when your network has many nodes with very few
links, or when you suspect significant missing data.

:::{toggle}
**Smart teleportation in detail**

The recorded node teleportation scheme commonly used with PageRank teleports
the walker to a uniformly random node. {cite:t}`lambiotte2012smart` showed
this creates artificial connections between modules: the teleportation
probability $\tau$ adds mixing between communities. In the map equation this
means that as $\tau$ increases, modules that are otherwise well-separated start
to look connected through the teleportation channel, eventually merging into
one flat cluster.

Unrecorded link teleportation (Infomap's default) avoids this by not encoding
teleportation steps in the module codebooks. The walker may still jump, but the
jump does not count as a module exit or entry. The stationary distribution is
$\pi_i^{\mathrm{unrec}} = \sum_l T_{il} \pi_{l;\alpha}$, i.e. the distribution
you would get by taking one more link-following step after a recorded walk.
Because only link-following steps define community membership, the partition
depends on the topology of the links, not on the value of $\tau$. Benchmarks
confirm this: with unrecorded teleportation the partition is essentially
independent of $\tau$ over a wide range, while recorded teleportation collapses
to a single module at high $\tau$.
:::

## The options that matter

One task per option, each on a small graph you can run as you read.

### Setting up

```{code-cell} python
import networkx as nx
import infomap
import random

# ── Helper: build an 8-clique ring (clear, clean structure) ──────────────────
G_ring = nx.ring_of_cliques(8, 5)   # 8 cliques × 5 nodes = 40 nodes

# ── Helper: build a weakly-structured graph (noisy, degenerate landscape) ───
def make_weak_sbm(n_communities=6, n_per=8, p_in=0.55, p_out=0.08, seed=123):
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

G_noisy = make_weak_sbm()
print(f"Ring of cliques: {G_ring.number_of_nodes()} nodes, {G_ring.number_of_edges()} edges")
print(f"Noisy SBM:       {G_noisy.number_of_nodes()} nodes, {G_noisy.number_of_edges()} edges")
```

### `seed` and reproducibility

`seed` fixes the internal random number generator. With the same seed you get
the same partition on every run, which makes results shareable and publishable.
The default `seed=123` is reproducible too, but setting the seed yourself ties
each result to the code that produced it.

```{code-cell} python
# Two runs with the same seed produce the same partition.
for attempt in range(2):
    result = infomap.run(G_ring, two_level=True, seed=123, num_trials=10, silent=True)
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
    result = infomap.run(G_noisy, two_level=True, seed=seed, num_trials=1, silent=True)
    print(f"  seed={seed}: L={result.codelength:.4f}, modules={result.num_top_modules}")
```

```{code-cell} python
# More trials explore more of the landscape and find a lower, stable minimum.
print("Fixed seed=123, varying num_trials:")
for num_trials in [1, 5, 20]:
    result = infomap.run(G_noisy, two_level=True, seed=123, num_trials=num_trials, silent=True)
    print(f"  num_trials={num_trials:2d}: L={result.codelength:.4f}, modules={result.num_top_modules}")
```

Notice how `num_trials=1` with `seed=123` lands at a higher codelength (worse
minimum) than `num_trials=20` with the same seed. The extra trials escape the
local trap and find a partition that compresses the random walk more
efficiently. The codelength converges: once you are reliably in the deepest
valley, adding more trials does not change the answer {cite:p}`calatayud2019solution`.

**Rule of thumb.** Use `num_trials=1` for quick exploration. Use
`num_trials=10` for most analyses. Use `num_trials=50` (or `converge=True`
with a higher cap) when you need high confidence in the global minimum or when
you are comparing partitions across networks.

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
    result = infomap.run(G_hier, two_level=two_level, seed=123, num_trials=10, silent=True)

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
# A directed graph: three directed cycles connected by directed bridges.
# The directed structure groups nodes by which cycle's flow they participate in.
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
                         seed=123, num_trials=10, silent=True)
    print(f"directed={directed}: modules={result.num_top_modules}, L={result.codelength:.4f}")
```

Both settings recover three modules here because the graph is strongly
clustered. On noisier real-world directed networks the difference is more
pronounced: the directed walk respects asymmetric flow and tends to find tighter
modules corresponding to true circulation patterns.

**Teleportation probability.** The default `teleportation_probability=0.15`
is the conventional PageRank value (one minus the 0.85 damping factor) and works
well for most networks. Because
Infomap uses unrecorded teleportation {cite:p}`lambiotte2012smart`, the partition
barely responds to this value, so the default is rarely worth changing unless
your domain says walkers teleport much more or less often.

### `markov_time` as a resolution dial

The map equation operates at the natural timescale of one random-walk step.
`markov_time` shifts this timescale: values below 1 encode the walk more
frequently (favoring more, smaller modules) and values above 1 encode it less
frequently (favoring fewer, larger modules). This is the principled way to
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
                         seed=123, num_trials=10, silent=True)
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

```{code-cell} python
# On the karate club, regularization merges weakly supported modules
# into fewer, larger communities.
G_karate = nx.karate_club_graph()

for label, kwargs in [
    ("baseline",                       {}),
    ("regularized",                    {"regularized": True}),
    ("regularized, strength=2",        {"regularized": True, "regularization_strength": 2.0}),
]:
    result = infomap.run(G_karate, two_level=True, seed=123, num_trials=10, silent=True, **kwargs)
    print(f"{label}: modules={result.num_top_modules}, L={result.codelength:.4f}")
```

Higher `regularization_strength` merges more modules. Use this when you have
prior reason to believe that small modules in your result reflect sampling noise
rather than real structure. The Bayesian derivation is in {cite:t}`smiljanic2020missing`.

### Why trials matter, visualised

A picture of the effect: run the noisy graph many times with a single trial each
(varying the seed) and compare those codelengths against the best of 20 trials.

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

# Codelength from one trial per seed ...
single_L = []
for s in range(1, 13):
    single_L.append(
        infomap.run(G_noisy, two_level=True, seed=s, num_trials=1, silent=True).codelength
    )

# ... versus the best of 20 trials.
best_L = infomap.run(
    G_noisy, two_level=True, seed=123, num_trials=20, silent=True
).codelength

fig, ax = plt.subplots(figsize=(6, 3.2))
ax.scatter(range(1, len(single_L) + 1), single_L,
           color="0.45", zorder=3, label="single trial (num_trials=1)")
ax.axhline(best_L, color="tab:blue", lw=2, label=f"best of 20 trials = {best_L:.3f}")
ax.set_xlabel("seed")
ax.set_ylabel("codelength (bits/step)")
ax.legend(loc="best", fontsize=8)
fig.tight_layout()
glue("fig-running-and-options", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-running-and-options
Each grey point is the codelength from a single trial with a different seed.
They scatter, and the higher ones sit above the best-of-20-trials value (blue
line): one trial can settle in a worse local minimum, while many trials reliably
reach the best partition. That gap is what `num_trials` buys you.
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
- **Codelength rises when you add metadata.** `result.codelength` reports only the
  topological term; the search still minimises the combined objective (see
  {doc}`/flow-models/metadata`).
- **More trials never hurt correctness, only runtime.** If repeated runs disagree,
  raise `num_trials` (or pass `converge=True`) rather than trusting one fit.
- **Sparse or under-sampled data over-splits.** Reach for `regularized=True`
  (see {doc}`/robustness/incomplete-data`).

## API pointers

The options in this chapter are keyword arguments to {func}`infomap.run` (and to
{class}`infomap.Infomap` and {meth}`infomap.Network.run`). After a run, the
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
| `silent` | bool | False | Suppress console output |

For the full set of options as a searchable table, see the
{class}`~infomap.Options` reference in the {doc}`/api/index`.

## See also

- **Options guide notebook** `examples/notebooks/options-guide.ipynb` lists every
  option in a searchable table, generated from the installed package so it matches
  your version. Start here for the exhaustive reference.
- {cite:t}`smiljanic2026survey`, §4, covers method selection and the flow-model
  justification.
- {cite:t}`calatayud2019solution` makes the quantitative case for multiple trials.
- {cite:t}`lambiotte2012smart` explains why unrecorded teleportation produces
  robust directed partitions.
- {cite:t}`kawamoto2015resolution` shows why the map equation's resolution limit is
  less restrictive than modularity's.
- {cite:t}`kheirkhahzadeh2016markov` introduces the Markov-time mechanism and its
  efficient resolution sweeps.
