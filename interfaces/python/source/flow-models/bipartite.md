---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Bipartite networks

{bdg-warning-line}`Flow model`

```{admonition} In one sentence
:class: tip
A bipartite network has two node types with links only between them. The
bipartite map equation exploits that alternating structure to compress
random-walk descriptions and reveal sharper community structure.
```

## Two node types, links between them

Many networks are not just "nodes and edges" but two distinct kinds of
entities connected to each other: users rating movies, species visiting
flowers, papers citing authors, proteins forming complexes. In all of
these cases links go exclusively *between* the two types, never within
them. This is a bipartite network.

The simplest way to handle bipartite networks in community detection is
to ignore the node-type information: project one type onto the other, or
apply a unipartite method directly. Both choices throw away structure.
Projections conflate distinct interaction patterns into weighted cliques;
applying a unipartite method to a bipartite network means the algorithm
cannot exploit the fact that a random walker *must* alternate between the
two types on every step.

That alternating constraint is a gift. In a unipartite network the walker
could in principle visit any pair of nodes in a row. In a bipartite network you
always know the *type* of the next node: leave a user and the next node is an
item, and the other way round. A coding scheme that uses that knowledge sends
shorter messages to describe the walk, and shorter messages mean more
compression and sharper module detection.

Infomap supports this natively. You assign integer node ids so that all type-A
nodes come first and all type-B nodes start at a threshold id, declare that
threshold by setting `im.bipartite_start_id`, and Infomap switches to the
bipartite map equation {cite:p}`blocker2020bipartite` for the optimisation.

## Two codebooks per module

In the standard map equation, each module has one codebook covering all
its nodes. The sender picks a codeword, and the receiver does not know in
advance whether the next node will be a user or an item.

With the bipartite map equation, each module gets *two* codebooks: one
for left-to-right transitions (e.g. user → item) and one for
right-to-left transitions (item → user). Because both sender and receiver
track the current node type, they always know which codebook applies next.
Halving the codebook population roughly halves the entropy, shortening
codewords and compressing the description.

The practical effect is higher resolution: the bipartite variant
partitions more finely, grouping nodes into tighter clusters that reflect
genuine co-use patterns rather than washing out across node types.
In all 21 real-world networks {cite:p}`blocker2020bipartite`, the
bipartite map equation compressed the walk description more than the
unipartite variant, and detected richer community hierarchies.

## The bipartite map equation

For an undirected bipartite graph with left nodes $N_L$ and right nodes
$N_R$, a random walker alternates between the two types. The standard map
equation treats all nodes symmetrically; the bipartite map equation
{cite:p}`blocker2020bipartite` separates each module's codebook into a
left-to-right part and a right-to-left part:

$$
L_B(\mathsf{M})
  = q^L H(\mathcal{Q}^L)
  + \sum_{\mathsf{m} \in \mathsf{M}} p^L_\mathsf{m}\, H(\mathcal{P}^L_\mathsf{m})
  + q^R H(\mathcal{Q}^R)
  + \sum_{\mathsf{m} \in \mathsf{M}} p^R_\mathsf{m}\, H(\mathcal{P}^R_\mathsf{m})
$$

where the superscripts $L$ and $R$ denote quantities computed separately
for left and right nodes. When there is no node-type information
($\mathcal{I} = 0$ bits) this reduces to the standard map equation;
with full node-type information ($\mathcal{I} = 1$ bit) you recover the
equation above. In between you can tune a node-type flipping rate $\alpha$ to
explore intermediate resolutions.

:::{toggle}
**Derivation sketch**

Apply Bayes' rule to the standard map equation. The random walk on a
bipartite network provides one extra bit of information per step: the
current node type. Knowing the type, the visit-rate entropy splits by
type:

$$
H(\mathcal{P}) = H(Y) + H(X|Y) = 1 + \tfrac{1}{2}H(\mathcal{P}^L) + \tfrac{1}{2}H(\mathcal{P}^R)
$$

where $Y$ is the node-type process (entropy exactly 1 bit because the
walk alternates deterministically) and $X|Y$ is the node conditional on
its type. Plugging this into the standard map equation and separating left
and right terms yields $L_B$. For each module $\mathsf{m}$,
$\mathcal{P}^L_\mathsf{m}$ is the set of left-node visit rates plus the
left-exit rate $q^L_\mathsf{m}$, and analogously for the right side.

For the mixed-memory variant, introduce a node-type flipping rate
$\alpha \in [0, \tfrac{1}{2}]$. The available node-type information is
$\mathcal{I}(\alpha) = 1 - H(\alpha, 1{-}\alpha)$. At $\alpha = 0$,
$\mathcal{I} = 1$ bit (full bipartite); at $\alpha = \tfrac{1}{2}$,
$\mathcal{I} = 0$ (standard map equation recovered). Intermediate values
sweep the resolution continuously; see {cite:p}`blocker2020bipartite`, §3, for the
full derivation.
:::

## Users and items in shared clusters

The scenario: four users and four items. Users 0 and 1 strongly prefer
items 4 and 5; users 2 and 3 strongly prefer items 6 and 7. A few weak
cross-cluster links add noise. We would like Infomap to recover the two
preference clusters, each containing two users and two items.

### Set up the network

```{code-cell} python
import infomap
from infomap import Network, run

# Node-id convention:
#   Users  → ids 0, 1, 2, 3         (type A: "left")
#   Items  → ids 4, 5, 6, 7         (type B: "right")
#
# All type-B ids must be >= BIPARTITE_START.
BIPARTITE_START = 4

net = Network()

# Declare the bipartite boundary: everything from id 4 onwards is type B.
net.bipartite_start_id = BIPARTITE_START

# Cluster 1: users {0,1} prefer items {4,5}
net.add_link(0, 4, 3.0)
net.add_link(0, 5, 2.0)
net.add_link(1, 4, 2.0)
net.add_link(1, 5, 3.0)

# Cluster 2: users {2,3} prefer items {6,7}
net.add_link(2, 6, 3.0)
net.add_link(2, 7, 2.0)
net.add_link(3, 6, 2.0)
net.add_link(3, 7, 3.0)

# Weak cross-cluster signal
net.add_link(0, 6, 0.2)
net.add_link(2, 4, 0.2)

print(f"Bipartite start id: {net.bipartite_start_id}")
print(f"Nodes with id >= {BIPARTITE_START} are treated as type-B (items).")
```

### Run and inspect modules

```{code-cell} python
result = run(net, two_level=True, num_trials=10, seed=123, silent=True)

modules = result.modules()   # {node_id: module_id}

print(f"Bipartite start id:      {net.bipartite_start_id}")
print(f"Top-level modules found: {result.num_top_modules}")
print(f"Codelength (bits/step):  {result.codelength:.4f}\n")

for node_id, module_id in sorted(modules.items()):
    kind = "user" if node_id < BIPARTITE_START else "item"
    print(f"  node {node_id:2d} ({kind:4s})  →  module {module_id}")
```

Both users *and* items in the same preference cluster land in the same
module: users {0, 1} group with items {4, 5}, and users {2, 3} with items
{6, 7}. The bipartite map equation
recovers the two-cluster structure even though it never sees a
user–user or item–item link; it exploits the alternating walk constraint
to group nodes by their co-occurrence patterns.

### Standard vs. bipartite map equation

Declaring the boundary changes the *objective*, not the data. Run the same links
without setting `bipartite_start_id`, and Infomap uses the standard (unipartite)
map equation instead:

```{code-cell} python
net_std = Network()
for u, v, w in [
    (0, 4, 3.0), (0, 5, 2.0), (1, 4, 2.0), (1, 5, 3.0),
    (2, 6, 3.0), (2, 7, 2.0), (3, 6, 2.0), (3, 7, 3.0),
    (0, 6, 0.2), (2, 4, 0.2),
]:
    net_std.add_link(u, v, w)
result_std = run(net_std, two_level=True, num_trials=10, seed=123, silent=True)

print(f"standard  map equation: {result_std.num_top_modules} modules, L={result_std.codelength:.4f}")
print(f"bipartite map equation: {result.num_top_modules} modules, L={result.codelength:.4f}")
```

Compare the two codelengths above. Across 21 real-world bipartite networks the
bipartite map equation compresses the walk further and resolves richer community
hierarchies than the unipartite variant {cite:p}`blocker2020bipartite`.

### Visualise

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

import networkx as nx
from docs_viz import draw_partition

# Build a NetworkX graph matching the Infomap network above
G = nx.Graph()
G.add_nodes_from(range(4))              # users
G.add_nodes_from(range(4, 8))           # items
for u, v, w in [
    (0, 4, 3.0), (0, 5, 2.0), (1, 4, 2.0), (1, 5, 3.0),
    (2, 6, 3.0), (2, 7, 2.0), (3, 6, 2.0), (3, 7, 3.0),
    (0, 6, 0.2), (2, 4, 0.2),
]:
    G.add_edge(u, v, weight=w)

flow = {n.node_id: n.flow for n in result.nodes()}
fig = draw_partition(G, modules, flow=flow)
glue("fig-bipartite", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-bipartite
A small weighted bipartite network (users 0-3, items 4-7) coloured by
module. The bipartite map equation groups each user with the items it
interacts with most, mixing the two node types within a module.
```

The two colours are the two preference clusters. The colour boundary does not
separate users from items; each coloured group spans both node types. That joint
clustering is the hallmark of bipartite community detection: a module is a
coherent user–item subgraph, not a user-only or item-only group.

## API pointers

- {attr}`infomap.Infomap.bipartite_start_id` declares the first node id of the
  second type: assign `im.bipartite_start_id = n` and every node with id `>= n`
  is a right node, ids below are left nodes. Set it before
  {meth}`~infomap.Infomap.run`. {attr}`infomap.Network.bipartite_start_id` is the
  same property on a :class:`~infomap.Network`.
- {meth}`infomap.Network.add_link` adds a weighted edge between any two node ids;
  for bipartite networks the source and target should be opposite types.
- {meth}`infomap.Result.modules` returns a `{node_id: module_id}` dict covering
  both types.
- The constructor flag `hide_bipartite_nodes=True` omits right-type nodes from
  the output, which helps when only the left-type assignments matter (projecting
  a user partition, say).

## Options

| Option | Where | Effect |
|---|---|---|
| `bipartite_start_id` | {class}`~infomap.Network` attribute | First node id of the second type; declares the network bipartite |
| `hide_bipartite_nodes` | {func}`infomap.run` | Omit the second-type nodes from the output |

## Going deeper

- The survey (§6.2) covers bipartite networks {cite:p}`smiljanic2026survey`.
- Companion notebook: `examples/notebooks/6.2 Bipartite Networks.ipynb` applies
  both the standard and bipartite map equations to a real weighted plant–ant web
  (Fonseca & Ganade 1996) and shows how the bipartite variant finds finer modules.
- Source paper {cite:p}`blocker2020bipartite`.
