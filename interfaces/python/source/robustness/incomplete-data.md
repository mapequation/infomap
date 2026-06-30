---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Incomplete data and regularization

```{admonition} In one sentence
:class: tip
When your network is missing links, standard Infomap overfits and splits nodes
into many small spurious modules. The Bayesian regularized map equation adds a
principled prior over transition rates so that only well-supported communities
survive.
```

## Motivation

Network data collected from real systems are almost always incomplete. A survey
captures a subset of acquaintances; a citation database crawls only a fraction
of references; a protein–protein interaction screen misses true binding partners.
When links are missing, the network is sparser than reality, and community
detection methods that take the observed network at face value will be misled.

The standard map equation is prone to this. It minimises a Shannon
entropy-based description length that systematically *underestimates* the true
entropy when data are sparse (Basharin 1959). As underestimation worsens when
communities lose links, the map equation's two cost terms fall out of balance:
the module codebooks look cheap to shrink, so the optimiser splits nodes into
smaller and smaller groups, down to groups of two or three nodes with no
principled support. Those modules are an artefact of noise, not real structure.

This problem is not about the search algorithm or the number of trials; it is a
bias in how the objective function responds to sparse data. The fix is a
regularization mechanism baked into the objective itself.

## Intuition

Think about what "community" means in information-theoretic terms: a group of
nodes where a random walker lingers. When the network is nearly complete, you
can trust the observed degree of each node as a reliable estimate of how
strongly it attracts the walker. When many links are missing, the observed
degree is just a noisy sample of the true degree. Relying on that sample
blindly inflates apparent differences between nodes, encourages the algorithm
to draw boundaries around statistical accidents, and produces many tiny
"communities" that would dissolve if you observed a few more links.

Regularization addresses this by blending what you observed with what you would
expect from an uninformative prior: a featureless, fully-connected prior network
with no community structure of its own. In sparse regions the prior pulls the transition rates
toward the flat background and evens out the apparent cost differences that drive
spurious splitting. Where evidence is abundant, the data dominate and the prior
barely matters.

The picture is similar to Bayesian smoothing for language models: a bigram
count of zero does not mean two words never co-occur; you add a small
pseudocount so the model is not overconfident about what it has not seen.

## Theory

{cite:t}`smiljanic2020missing` introduced the regularized map equation for
undirected, unweighted networks; {cite:t}`smiljanic2021incomplete` extended it to
weighted and directed networks. Both are implemented in Infomap under the single
flag `regularized=True`.

The mechanism is Bayesian. Rather than trust the observed network at face value,
Infomap blends it with a fully connected **prior network** that carries no
community structure, the continuous configuration model. The random walker then
moves on the combination: most steps follow observed links, but with a small,
data-dependent probability the walker takes a prior step toward any node. Nodes
with few observed links lean more on the prior; nodes with many links lean on
their data. Where evidence is abundant the data dominate and the prior barely
matters; where the network is sparse the prior evens out the apparent cost
differences that drive spurious splitting.

The strength of the prior is $\lambda = C\,\ln N / N$, where $N$ is the number of
nodes and $C$ is the `regularization_strength` (default $1$). The value
$\ln N / N$ is the edge probability at which an Erdős–Rényi random graph becomes
almost surely connected, which is what makes it a principled default (see the
toggle).

:::{toggle}
**Why $\lambda = \ln N / N$**

An Erdős–Rényi random graph on $N$ nodes with edge probability $\lambda$
undergoes a connectivity phase transition at $\lambda = \ln N / N$: below it the
graph almost surely breaks into isolated components, so a weaker prior cannot
stop the map equation from giving poorly-sampled nodes their own spurious
modules; above it the prior network is connected but still structureless, so it
adds no bias toward any grouping. The default $C = 1$
(`regularization_strength=1.0`) places the prior exactly at this critical point,
balancing resistance to overfitting against washing out real communities. Lower
values (e.g. $C = 0.5$) allow more communities; higher values push toward fewer.
For severely undersampled networks the regularized objective prefers the
one-module solution, reporting that the evidence does not support any community
structure.

A separate option, `entropy_corrected`, corrects the small-sample bias in the
entropy estimate itself; it is independent of the prior-network regularization
described here.
:::

## A worked example

We construct a synthetic network with five planted communities of twelve
nodes each using a stochastic block model: edges appear within communities
with probability 0.7 and between communities with probability 0.02. We then
remove 60 % of the edges uniformly at random to simulate incomplete
observations.

### Build the network

```{code-cell} python
import networkx as nx
import infomap
import random

# Reproducible construction
random.seed(99)

n_communities = 5
n_per_community = 12

# Ground truth: 5 modules of 12 nodes each
g_full = nx.stochastic_block_model(
    sizes=[n_per_community] * n_communities,
    p=[[0.7 if i == j else 0.02 for j in range(n_communities)]
       for i in range(n_communities)],
    seed=99,
)
print(f"Full graph : {g_full.number_of_nodes()} nodes, {g_full.number_of_edges()} edges")

# Remove 60 % of edges to simulate missing link observations
edges = list(g_full.edges())
random.shuffle(edges)
removal_fraction = 0.70
g_sparse = g_full.copy()
g_sparse.remove_edges_from(edges[: int(removal_fraction * len(edges))])
print(f"Sparse graph: {g_sparse.number_of_nodes()} nodes, {g_sparse.number_of_edges()} edges")
print(f"({removal_fraction:.0%} of links removed)")
```

### Run standard vs. regularized Infomap

```{code-cell} python
# Standard Infomap: no regularization
result_std = infomap.run(g_sparse, two_level=True, seed=99, num_trials=10, silent=True)

# Regularized Infomap: Bayesian map equation with regularization_strength=0.5
result_reg = infomap.run(
    g_sparse,
    two_level=True,
    seed=99,
    num_trials=10,
    silent=True,
    regularized=True,
    regularization_strength=0.5,
)

modules_std = result_std.modules()
modules_reg = result_reg.modules()

print(f"Ground truth  : {n_communities} communities")
print(f"Standard      : {result_std.num_top_modules} modules  (codelength {result_std.codelength:.4f} bits/step)")
print(f"Regularized   : {result_reg.num_top_modules} modules  (codelength {result_reg.codelength:.4f} bits/step)")
```

On this 70 %-sparse graph standard Infomap over-partitions: it reports many more
modules than the five planted communities (see the counts above), because the
objective underestimates the true description length on a sparse graph and
commits to a finer partition than the data warrant. Regularization merges the
noise-driven splits and pulls the count back toward five. It does not always land
exactly on the ground truth, and at this severe sparsity the principled default
`regularization_strength=1.0` over-regularizes, collapsing toward a single
module, which is why this example uses a gentler `0.5`.

```{admonition} Module size distributions
:class: note
The spurious modules standard Infomap adds are small groups of nodes that lost
most of their links through random removal, leaving them weakly connected to any
community. Regularization treats those nodes as part of the nearest genuine
module because the prior pseudocounts smooth out the sparse links.
```

### Visualise the regularized partition

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

from docs_viz import draw_partition

flow = {n.node_id: n.flow for n in result_reg.nodes()}
fig = draw_partition(g_sparse, modules_reg, flow=flow)
glue("fig-incomplete-data", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-incomplete-data
A sparse network partitioned with the regularized map equation
(regularized=True). The Bayesian prior keeps weakly-sampled nodes in larger
modules instead of breaking them into noise-driven singletons.
```

Each colour is one of the five recovered modules. Even with 60 % of links
missing, the regularized map equation finds groups that match the planted
partition, because the prior absorbs the statistical noise that would otherwise
attract spurious module boundaries.

## API pointers

Pass `regularized=True` to {func}`infomap.run` to activate the Bayesian map
equation. One optional keyword controls the prior strength:

- `regularized=True` enables the Bayesian regularized map equation, which adds a
  fully connected prior network to reduce overfitting to missing links. It works
  for undirected, weighted, and directed networks
  ({cite:p}`smiljanic2020missing` for undirected unweighted;
  {cite:p}`smiljanic2021incomplete` for weighted and directed).
- `regularization_strength=` (float, default `1.0`) is the constant $C$ scaling
  the prior strength $\lambda = C\,\ln N / N$. The default places the prior at the
  Erdős–Rényi
  connectivity threshold. Lower values (e.g. `0.5`) are more permissive and find
  more communities; higher values (e.g. `2.0`) impose a stronger prior and tend
  toward fewer modules. Start with the default and adjust only when you have
  reason to trust community boundaries in sparse data ($C < 1$) or to suppress
  them harder ($C > 1$).

All other engine options (`num_trials`, `seed`, `two_level`, `directed`, and so
on) compose freely with `regularized=True`.

- {attr}`infomap.Result.codelength` is the map equation value for the winning
  partition; with regularization it reflects the Bayesian-corrected description
  length.
- {attr}`infomap.Result.num_top_modules` is the number of top-level modules
  found.
- {meth}`infomap.Result.modules` returns a `{node_id: module_id}` dict.

## Going deeper

- {cite:t}`smiljanic2026survey`, §7, sets incomplete data, overfitting, and the
  regularized map equation in context.
- Companion notebook: `examples/notebooks/7 Networks with incomplete data.ipynb`
  runs link-removal experiments with cross-validation and a degree-corrected
  stochastic block model comparison.
- {cite:t}`smiljanic2020missing` introduces the regularized map equation for
  undirected unweighted networks.
- {cite:t}`smiljanic2021incomplete` extends it to weighted and directed networks
  via an empirical Bayes estimate of transition rates.
- {cite:t}`neuman2025reliable` scores how reliable each module is, so you can tell
  a robust partition from one driven by sampling noise.
