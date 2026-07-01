---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Flow and random walks

{bdg-info-line}`Concept`

```{admonition} In one sentence
:class: tip
Infomap finds communities by asking where a random walker lingers. Flow is that
walker's stationary visit frequency, and modules are the regions that trap it.
```

## What flow captures

Every network encodes a pattern of movement. Hyperlinks carry web surfers from
page to page, citations pull readers toward influential papers, synaptic
connections route neural signals. Revealing the large-scale structure of such a
system takes a currency that respects direction, weight, and how paths chain
together, not only which edges are present.

Counting edges inside and outside groups misses something dynamic: the way a
random walker entering a dense cluster tends to stay there for many steps before
it escapes. That persistence is what defines a community in flow-based clustering: a module
is a part of the network where flow **lingers** {cite:p}`rosvall2008maps`.

The map equation, Infomap's objective function, measures how far a description
of the walker's trajectory compresses. To do that it needs to know how often the
walker visits each node and how often it crosses module boundaries. Both
come from **flow**, so flow is the first thing to understand about why Infomap
partitions a network the way it does.

## Where the walker lingers

Picture a walker that starts at some node and, at each step, follows one of the
outgoing edges at random. It spends more time in densely connected regions and
now and then crosses into another. Track where it sits over a long run, and the
fraction of time spent at each node is that node's **flow**, its stationary
visit frequency.

The modules are the regions where the walker stays for many steps before it
moves on. Inside such a region the walker's path is long and locally
predictable, broken only by rare crossings. A two-level code exploits those long
stretches: reuse short codewords inside each region, and pay a brief exit toll
only when the walker crosses to another. The partition with the shortest average
toll is the one Infomap returns.

**Directed** networks need one more ingredient. A directed graph can have
dangling nodes (no out-edges) or sink components the walker can never leave, so
the walk fails to reach every node; it is not *ergodic*. The fix is
**teleportation**: with small probability $\tau$ (default 0.15, the conventional
PageRank value) the walker teleports to another node instead of following an edge.
That guarantees a unique stationary distribution, but the jumps add long-range
links that blur module boundaries. {cite:t}`lambiotte2012smart` showed that
*unrecorded* teleportation, which leaves the jump steps out of the recorded
trajectory, removes most of the blurring and makes the partition robust to the
value of $\tau$. Infomap uses unrecorded teleportation by default for directed
networks.

## Flow as a stationary distribution

A directed, weighted network on $n$ nodes defines a transition matrix
$T_{ij} = w_{ij} / w_j^{\text{out}}$, where $w_{ij}$ is the edge weight from
$j$ to $i$ and $w_j^{\text{out}}$ is node $j$'s total out-weight.  The
**stationary distribution** $\pi$ satisfies

$$
\pi_i = \sum_j T_{ij}\,\pi_j,
$$

and gives the fraction of time a long random walk spends at node $i$. Infomap
calls this the **flow** of node $i$. High-flow nodes sit at the network's
convergence points; many paths run through them, so the walker visits them
often.

With teleportation at rate $\tau$ the effective transition is

$$
p_{i,\,t+1} = (1 - \tau)\sum_j T_{ij}\,p_{j,t} + \tau\,\frac{1}{n},
$$

and the unique stationary solution $\pi$ exists for any $\tau > 0$. The $1/n$
term is the simplest, uniform teleportation; by default Infomap teleports to a
node with probability proportional to its in-strength. Infomap also uses
*unrecorded* teleportation {cite:p}`lambiotte2012smart`: the teleportation hops
are not counted in the code length, so they do not artificially inflate
cross-module traffic.

These per-node flows $\pi_i$, together with how often the walker crosses between
candidate modules, are everything the objective needs. The next chapter,
{doc}`the-map-equation`, turns them into the quantity Infomap actually minimises;
here we only establish the flow it is built from.

:::{toggle}
**Computing the flow**

For an unweighted undirected network the transition matrix is
$T_{ij} = A_{ij}/k_j$ where $k_j$ is the degree of node $j$, and the
stationary distribution is $\pi_i = k_i / (2|E|)$, proportional to degree.
For a weighted directed network $\pi$ is the leading right eigenvector of $T$,
which the power method computes iteratively.

For directed networks with teleportation, Infomap first computes $\pi$ for the
recorded walk (including teleportation steps), then re-weights it to count only
link-traversal steps (the unrecorded scheme), so teleportation does not inflate
the flow across module boundaries. See {cite:t}`lambiotte2012smart` for the
formal derivation.
:::

## Two cycles joined by a one-way bridge

We build a tiny directed network by hand: two tight 3-cycles connected by a
single bridge edge.

```{code-cell} python
import networkx as nx
import infomap

# Two directed 3-cycles: A = {0,1,2} and B = {3,4,5}
G = nx.DiGraph()
G.add_edges_from([(0, 1), (1, 2), (2, 0)])   # cycle A
G.add_edges_from([(3, 4), (4, 5), (5, 3)])   # cycle B
G.add_edge(2, 3)                              # bridge: A leaks into B

print("Nodes:", list(G.nodes()))
print("Edges:", list(G.edges()))
```

```{code-cell} python
# Run Infomap on the directed network
result = infomap.run(G, directed=True, silent=True, seed=123, num_trials=10)

# Read per-node flow (stationary visit frequency) and module assignments
flow    = {node.node_id: node.flow       for node in result.nodes()}
modules = {node.node_id: node.module_id  for node in result.nodes()}

print(f"Modules found : {result.num_top_modules}")
print(f"Codelength    : {result.codelength:.4f} bits/step")
print()
print(f"{'Node':>5}  {'Cycle':>7}  {'Module':>8}  {'Flow':>8}")
print(f"{'----':>5}  {'-----':>7}  {'------':>8}  {'------':>8}")
labels = {0: 'A', 1: 'A', 2: 'A', 3: 'B', 4: 'B', 5: 'B'}
for nid in sorted(flow):
    print(f"{nid:>5}  {labels[nid]:>7}  {modules[nid]:>8}  {flow[nid]:>8.4f}")
```

Nodes in cycle B carry higher flow than nodes in cycle A, even though the two
cycles have the same topology. The bridge `2 → 3` makes the difference: the
walker can drift from A into B but has no edge back. Teleportation is the only
return route, so cycle A collects fewer link-following visits and its flow stays
low. Infomap still puts each cycle in its own module, because each one is tight
relative to its links to the outside.

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

from docs_viz import draw_partition

fig = draw_partition(G, modules, flow=flow)
glue("fig-flow-and-random-walks", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-flow-and-random-walks
A small network coloured by module. A random walker stays inside one colour
for long stretches and crosses to the other only occasionally; those
persistent visits are the flow the map equation compresses.
```

The two colours are the two detected modules. The bridge edge crosses the colour
boundary, where flow leaks from one community into the other.

```{admonition} Teleportation and directed ergodicity
:class: note
Without teleportation a directed walk can get stuck in cycle B and never return
to A (no edge leaves B). Teleportation at rate $\tau = 0.15$ restores ergodicity
so every node stays reachable, while Infomap's unrecorded scheme keeps those
artificial hops from inflating the apparent flow across module boundaries. The
community structure then reflects the *link topology* rather than the
teleportation bookkeeping {cite:p}`lambiotte2012smart`.
```

## API pointers

- {func}`infomap.run` is the entry point; pass `directed=True` to use the
  directed flow model with unrecorded teleportation. It returns a
  {class}`~infomap.Result`.
- {meth}`infomap.Network.from_networkx` loads a `networkx.DiGraph` (or `Graph`)
  for non-default options; passing the graph straight to {func}`infomap.run`
  works for the common case.
- {meth}`infomap.Network.add_link` adds individual edges programmatically.
- {meth}`infomap.Result.nodes` iterates per-node views; each exposes `.node_id`,
  `.module_id`, and `.flow`.
- {meth}`infomap.Result.to_dataframe` returns `flow`, `module_id`, `node_id`
  (and more) as a pandas DataFrame in one call.
- {attr}`infomap.Result.codelength` is the map-equation value at the partition
  found, in bits per step.

## Going deeper

- {cite:t}`smiljanic2026survey`, §2–3, for the broader treatment.
- {cite:t}`rosvall2008maps` is the source paper for flow-based community detection.
- {cite:t}`lambiotte2012smart` covers teleportation in directed networks.
- Companion material: the {doc}`survey companion notebooks </article-companion/index>`
  demonstrate flow on larger real-world networks.
