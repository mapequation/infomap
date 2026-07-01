---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# The map equation

{bdg-info-line}`Concept`

```{admonition} In one sentence
:class: tip
The map equation measures how many bits it takes to describe a random walk on
your network. That number is the *codelength*; Infomap finds the partition that
makes it smallest, which is the partition where flow stays trapped in modules.
```

## Two questions you can ask of a network

When you split a network into communities, how do you know the split is *good*?
You need a quality function: a single number that scores any candidate partition.

Many methods use modularity, which counts whether more edges fall inside modules
than a random baseline would predict. Modularity asks a question about how the
network was wired.

The map equation asks a question about how it is *used*. Given that flow moves
through the network, passengers through airports, clicks across the web, messages
through a social graph, which partition best compresses a description of that
movement? The answer is the partition with the shortest **codelength**: the
fewest bits per step needed to describe a random walk. A short codelength means
the partition captures real structure in the flow. Both questions are legitimate
and often agree; they simply differ, and when links carry movement the two views
can disagree, with the map equation the one built for the flow.

## Reusing street names

Picture narrating a random walker's journey to a friend, one step at a time, in
as few words as possible. The trick is the one paper maps already use: reuse
names.

Street names repeat across cities. Almost every town has a Main Street. Within
one city you say the short local name and announce the city only when you cross
into a new one. Stay inside a city and you spend the day saying short street
names; you rarely pay to name the city.

A network with communities works the same way. Give each module its own small
**module codebook** of short codewords for the nodes inside it, and add one tiny
**index codebook** whose only job is to announce "switch to module 2." If the
walker wanders inside a module for long stretches and crosses between modules
only rarely, you almost always speak in short module codewords and seldom pay for
the index. The description shrinks. A partition that ignores the real structure
keeps the walker crossing boundaries, so you pay the index cost constantly and
the description grows.

The map equation, $L(\mathsf{M})$, is the average description length per step
under the best two-level code for partition $\mathsf{M}$. **Minimising $L$ over
all partitions is how Infomap defines and finds communities: the partition that
best compresses the flow.**

## The map equation, term by term

For a partition $\mathsf{M}$ into $m$ modules, the map equation is the minimum
average bits per step for a two-level code of an infinite random walk:

$$
L(\mathsf{M}) =
  \underbrace{q_{\curvearrowright} H(\mathcal{Q})}_{\text{between modules}}
  +
  \underbrace{\sum_{i=1}^{m} p_{\circlearrowright}^i H(\mathcal{P}^i)}_{\text{within modules}}
$$

The two terms are the two codebooks:

- **Between-module (index) term.** $q_{\curvearrowright}$ is how often the walker
  switches modules; $H(\mathcal{Q})$ is the entropy of the module-name codebook.
  Together: the cost of announcing module crossings.
- **Within-module term.** $p_{\circlearrowright}^i$ is the fraction of steps that
  use module $i$'s codebook (visits inside it, plus its exit signal);
  $H(\mathcal{P}^i)$ is that codebook's entropy. Summed over modules: the cost of
  naming nodes within modules.

The best partition balances the two. Too many modules and the between term grows,
because the walker keeps crossing boundaries. Too few and the within term grows,
because each codebook must name many nodes with long codewords. The minimum of
$L$ lands on the community structure the flow reveals. **Lower codelength = better
compression = more pronounced community structure with respect to the flow**, so
when you compare two partitions of the same network, the smaller $L$ wins.

For undirected networks, node visit frequencies equal normalised link weights, so
Infomap needs no teleportation. For directed networks Infomap uses a random-surfer
model with teleportation probability $\tau = 0.15$ (the conventional choice, one
minus PageRank's 0.85 damping factor) to guarantee an ergodic stationary
distribution.

:::{toggle}
**Full term-by-term form**

The two-level code {cite:p}`rosvall2009map` uses the index codebook at
total exit rate $q_{\curvearrowright} = \sum_i q_{i\curvearrowright}$, with entropy

$$
H(\mathcal{Q}) = -\sum_{i=1}^m
  \frac{q_{i\curvearrowright}}{q_{\curvearrowright}}
  \log\!\left(\frac{q_{i\curvearrowright}}{q_{\curvearrowright}}\right),
$$

and module $i$'s codebook (one codeword per node plus an exit symbol, with
$p_{\circlearrowright}^i = q_{i\curvearrowright} + \sum_{\alpha\in i} p_\alpha$) has
entropy

$$
H(\mathcal{P}^i)
= -\frac{q_{i\curvearrowright}}{p_{\circlearrowright}^i}
    \log\!\frac{q_{i\curvearrowright}}{p_{\circlearrowright}^i}
  -\sum_{\alpha\in i}\frac{p_\alpha}{p_{\circlearrowright}^i}
    \log\!\frac{p_\alpha}{p_{\circlearrowright}^i}.
$$

By Shannon's source-coding theorem {cite:p}`shannon1948mathematical`, entropy is a
hard lower bound on average codeword length, so $L(\mathsf{M})$ is the shortest
possible two-level description per step. Combining and simplifying gives a form
that updates cheaply when one node moves between modules, by tracking only
$q_{i\curvearrowright}$ and $\sum_{\alpha\in i} p_\alpha$ per module; see
{cite:t}`rosvall2009map` for the fast stochastic search that exploits it.
:::

## Does compression reveal the karate club's split?

Zachary's karate club is a classic benchmark: 34 people, 78 friendships, and a
known split into two factions. It is small enough to explore interactively and
has real structure Infomap reliably recovers.

```{code-cell} python
import networkx as nx
import infomap

g = nx.karate_club_graph()
print(f"Nodes: {g.number_of_nodes()}, Edges: {g.number_of_edges()}")

# Two-level search, 10 independent trials, fixed seed for reproducibility.
result = infomap.run(g, two_level=True, seed=123, num_trials=10, silent=True)

modules = result.modules()            # {node_id: module_id}
n_top   = result.num_top_modules
L       = result.codelength           # the map equation value for the best partition
L_one   = result.one_level_codelength # cost with all nodes in one module

print(f"\nCodelengths (bits per step):")
print(f"  One-level (no modules):           {L_one:.4f}")
print(f"  Infomap partition ({n_top} modules): {L:.4f}")
print(f"  Compression gain:                 {(L_one - L) / L_one * 100:.1f}%")
```

The modular description wins: the one-level codelength, the price of describing
every node with one flat codebook, runs higher than the Infomap codelength. That
gap is the evidence the karate club has community structure. The walker does not
spread evenly; it lingers in pockets, and naming those pockets shortens the
description.

```{admonition} Why more than two modules?
:class: note
The club famously split into two factions, but Infomap minimises the description
length of the flow, not a sociological label. Nodes on the boundary between the
factions can form their own transitional cluster where the walker's affiliation
is split, and naming it shortens the code, so Infomap often reports more than two
modules here. Whether that extra group means something is yours to judge. If you
need exactly two modules, pass `preferred_number_of_modules=2`.
```

```{code-cell} python
import matplotlib.pyplot as plt
from docs_viz import draw_partition
from myst_nb import glue

flow = {n.node_id: n.flow for n in result.nodes()}
fig = draw_partition(g, modules, flow=flow)
glue("fig-map-equation", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-map-equation
Zachary's karate club, coloured by the modules Infomap finds. The walker lingers
inside each colour and crosses between them only rarely, which is exactly the
structure the map equation compresses. The boundary nodes form a small extra
module because naming it shortens the overall code.
```

## API pointers

- {func}`infomap.run` is the entry point; pass `two_level=True` to constrain the
  search to a flat (non-hierarchical) partition. It returns a
  {class}`~infomap.Result`.
- `result.codelength` is the per-step description length in bits for the best
  partition found; `result.one_level_codelength` is the cost with no modules, and
  `result.relative_codelength_savings` reports the gain between them.
- `result.num_top_modules` counts the top-level modules; `result.modules()`
  returns a `{node_id: module_id}` mapping.
- `preferred_number_of_modules=k` softly steers the search toward `k` modules.

## Going deeper

- {cite:t}`smiljanic2026survey`, §3, derives the map equation and compares it with
  modularity in depth.
- Companion notebook: `examples/notebooks/3.1 The two-level map equation.ipynb`.
- {cite:t}`rosvall2008maps` is the source paper; {cite:t}`rosvall2009map` is a
  longer pedagogical account with the fast stochastic search algorithm.
