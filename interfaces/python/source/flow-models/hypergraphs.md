---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Hypergraphs and higher-order networks

```{admonition} In one sentence
:class: tip
A hyperedge can connect any number of nodes at once, but to apply Infomap you
first decide how to represent that multi-body interaction as a network: the
choice of representation and random-walk model changes the communities you find
{cite:p}`eriksson2021hypergraph`.
```

## Motivation

Most network models assume that every interaction involves exactly two
parties: Alice emails Bob, city A is connected to city B, gene X regulates
gene Y. But many real systems have genuinely multi-body interactions: a paper
is co-authored by five people at once; a chemical reaction involves three
molecules; a group text connects a whole team. A **hypergraph** captures this
directly: a *hyperedge* connects any number of nodes, not only a pair.

The multi-body structure matters for community detection. Suppose three
researchers jointly author dozens of papers. A pairwise projection represents
this as three separate edges, but the joint authorship creates a shared context,
a flow pattern, that a pairwise edge misses. Hyperedges let you encode that
context explicitly.

The catch: Infomap clusters a *network*, not a hypergraph directly. You must
translate the hypergraph into a network first, and the translation is not
unique. As Eriksson, Edler, Rojas, de Domenico and Rosvall (2021) showed
systematically, three broad families of representation exist (unipartite,
bipartite, and multilayer), and each implies a different random-walk model,
different link-flow volumes, and so different community structure. The choice of
representation is a modelling decision in its own right, not a preprocessing
detail.

## Intuition

Think of a co-authorship hyperedge as a room: every author in it is standing
in the same room. A random walker in the room picks a colleague at random and
follows them to the next room (the next paper). How you model the room changes
the walk:

- **Unipartite (clique expansion):** Replace each room with a complete graph
  among its occupants. The walker moves directly from person to person. Simple,
  but inflates the link count quadratically with hyperedge size, and the flow
  between co-authors in a large hyperedge is diluted.

- **Bipartite (explicit hyperedge nodes):** Add a node for the room itself.
  The walker steps *node → room → node*. Hyperedge membership is represented
  explicitly, the link count grows linearly with hyperedge size, and community
  detection also assigns the room nodes to modules.

- **Multilayer (one layer per hyperedge):** Give each room its own floor of a
  building. The walker moves within a floor among that paper's co-authors,
  then occasionally takes the stairs to a different floor. Allows flows to
  linger within hyperedges and supports overlapping communities.

The key insight from {cite:t}`eriksson2021hypergraph` is that all three representations
can be designed to give identical *node-visit rates* for the same random-walk
model, yet they produce different *link flows* and therefore different optimal
partitions. Bipartite representations tend to favour fewer, larger modules;
unipartite and multilayer representations resolve finer structure.

## Theory

Let $\mathcal{H}(V, E)$ be a hypergraph with node set $V$ and hyperedge set
$E$. A random walk on $\mathcal{H}$ proceeds in two explicit steps: from node
$u$, pick a hyperedge $e \in E(u)$ with probability proportional to its weight
$\omega(e)$, then pick a destination node $v \in e$ with probability
proportional to its node weight $\gamma_e(v)$ within that hyperedge.

The three network representations encode this two-step walk differently
(Eriksson et al. 2021, Table 1):

| Representation | Links grow as | Modules include | Supports overlap |
|---|---|---|---|
| Unipartite (clique expansion) | $O(|e|^2)$ per hyperedge | Original nodes only | No |
| Bipartite (explicit hyperedge nodes) | $O(|e|)$ per hyperedge | Nodes + hyperedge-nodes | No |
| Multilayer (one layer per hyperedge) | $O(|e|^2)$ per layer | State nodes (node × layer) | Yes |

For the **bipartite** representation, the transition probabilities factor
naturally:

$$P_{u \to e} = \frac{\omega(e)}{d(u)}, \qquad P_{e \to v} = \frac{\gamma_e(v)}{\delta(e)},$$

where $d(u) = \sum_{e \in E(u)} \omega(e)$ is the total incident hyperedge
weight of $u$ and $\delta(e) = \sum_{v \in e} \gamma_e(v)$ is the total node
weight within $e$. Infomap treats the bipartite network as any weighted
directed graph, except that hyperedge-nodes are flagged as a second node type
via `bipartite_start_id`; their stationary flow is then redistributed back to
the regular nodes before the map equation is evaluated, so the codelength
reflects only the original-node community structure.

:::{toggle}
**Unipartite projection and hyperedge-size bias**

The unipartite transition rate between nodes $u$ and $v$ is the marginalised
two-step rate:

$$P_{uv} = \sum_{e \in E(u,v)} P_{u \to e} \cdot P_{e \to v} = \sum_{e \in E(u,v)} \frac{\omega(e)}{d(u)} \frac{\gamma_e(v)}{\delta(e)},$$

where $E(u,v)$ is the set of hyperedges containing both $u$ and $v$. Each
hyperedge becomes a weighted clique.

Eriksson, Carletti, Lambiotte, Rojas and Rosvall (2022) extended this to a
*size-biased* random walk with parameter $\sigma$: by setting
$\omega(E_\alpha) \propto (|E_\alpha| - 1)^{\sigma+1}$, the walker is biased
toward large hyperedges ($\sigma > 0$) or small ones ($\sigma < 0$). At
$\sigma = 0$ the walk reduces to the clique-expanded multigraph. This
parameterisation lets the researcher encode an explicit hypothesis about
whether large multi-body interactions or small ones drive the flow.

The **multilayer** representation creates one layer per hyperedge. Node $u$'s
*state node* in layer $\alpha$ captures how the walker behaves specifically
while it is in hyperedge $\alpha$. The multilayer coupling strength between
similar layers can be set proportional to the Jensen-Shannon divergence
between the layers' outgoing transition vectors, creating
*hyperedge-similarity walks* that linger among topically similar hyperedges
and can produce deeply nested, overlapping community structures.
:::

## A worked example

We construct a small hypergraph with two clear communities connected by a
single bridge hyperedge, then encode it as a bipartite incidence network and
run Infomap.

**The hypergraph:**
- Community A: nodes {0, 1, 2, 3}, hyperedges $H_0 = \{0,1,2\}$ and $H_1 = \{1,2,3\}$
- Community B: nodes {4, 5, 6, 7}, hyperedges $H_2 = \{4,5,6\}$ and $H_3 = \{5,6,7\}$
- Bridge: $H_4 = \{3, 4\}$

**Bipartite encoding:** assign regular nodes IDs 0–7 and hyperedge-nodes IDs 8–12.
Every membership $(n, H_k)$ becomes a directed edge in both directions, forming
an undirected bipartite graph. Setting `bipartite_start_id = 8` tells Infomap
that nodes 8 and above are hyperedge-nodes whose flow should be folded back
into the regular-node codelength.

```{code-cell} python
import infomap
import networkx as nx

# --- Define the hypergraph ---
# Keys are hyperedge-node IDs; values are the member node IDs.
hyperedges = {
    8:  [0, 1, 2],   # H0: community A
    9:  [1, 2, 3],   # H1: community A
    10: [4, 5, 6],   # H2: community B
    11: [5, 6, 7],   # H3: community B
    12: [3, 4],      # H4: bridge
}
n_nodes = 8          # regular nodes: 0–7
bipartite_start = 8  # hyperedge-nodes start here

# --- Build the bipartite incidence graph (for visualisation) ---
G = nx.Graph()
G.add_nodes_from(range(n_nodes))
G.add_nodes_from(range(bipartite_start, bipartite_start + len(hyperedges)))
for he_id, members in hyperedges.items():
    for n in members:
        G.add_edge(n, he_id)

# --- Run Infomap on the bipartite encoding ---
im = infomap.Infomap(two_level=True, num_trials=10, seed=123, silent=True)
for he_id, members in hyperedges.items():
    for n in members:
        im.add_link(n, he_id)

im.bipartite_start_id = bipartite_start
im.run()

modules = im.get_modules()          # includes both regular and hyperedge-nodes
node_modules = {n: modules[n] for n in range(n_nodes)}

print(f"Top-level modules found: {im.num_top_modules}")
print(f"Codelength:              {im.codelength:.4f} bits/step")
print(f"Node → module:           {node_modules}")
```

Infomap finds two modules that correctly separate community A (nodes 0–3) from
community B (nodes 4–7), even though the bridge hyperedge $H_4$ links nodes 3
and 4. The hyperedge-nodes are also assigned to modules: $H_0$, $H_1$, and
$H_4$ land in module 1 alongside their member nodes; $H_2$ and $H_3$ land in
module 2. The bridge hyperedge joins the module of the community that supplies
most of its flow.

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

from docs_viz import draw_partition

flow = {n.node_id: n.data.flow for n in im.nodes}
fig = draw_partition(G, modules, flow=flow)
glue("fig-hypergraphs", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-hypergraphs
The hypergraph encoded as a bipartite incidence network and coloured by
module. Each hyperedge becomes its own node linking the members it joins, so
multi-body interactions turn into ordinary two-node flow.
```

The visualisation shows the bipartite structure directly: the eight circular
node-positions (left and right clusters) are the regular nodes; the five square
positions are the hyperedge-nodes. Colour indicates module, and the two
communities stay separate across the bridge.

```{admonition} Interpreting the bipartite result
:class: note
When you call `im.get_modules()` on a bipartite run, the returned dictionary
contains entries for both regular nodes and hyperedge-nodes. If you only care
about the original-node partition, filter to `{n: modules[n] for n in range(n_nodes)}`.
The codelength printed by Infomap already accounts for only the regular-node
flow; hyperedge-node flows are folded back before the map equation is evaluated.
```

## Beyond bipartite: full flow-based hypergraph models

The bipartite approach above is the most practical path when you are working
directly with the Infomap Python package: it requires only the standard
`add_link` / `bipartite_start_id` API and runs on any installed version.

For richer flow models (non-lazy walks, hyperedge-size-biased walks, and the
multilayer hyperedge-similarity walks that enable overlapping community
detection), the **`mapping-hypergraphs`** tool (implemented in Rust by the
MapEquation team) automates the conversion from a raw hyperedge list to the
appropriate bipartite, unipartite, or multilayer Infomap input. You point it
at your data, choose a representation and walk model, and it writes out the
network file ready for Infomap.

A schematic of the full workflow (not executed here because it requires the
external tool):

```python
# Illustrative only; requires the mapping-hypergraphs tool:
# https://github.com/mapequation/mapping-hypergraphs
#
# 1. Prepare a hyperedge list, e.g.:
#       0 1 2        # hyperedge H0
#       1 2 3        # hyperedge H1
#       4 5 6        # hyperedge H2
#
# 2. Convert to a bipartite Infomap input:
#    $ mapping-hypergraphs --representation bipartite \
#                          --walk lazy             \
#                          hyperedges.txt > network.net
#
# 3. Run Infomap on the generated network:
import infomap
im = infomap.Infomap("--two-level --num-trials 10")
im.read_file("network.net")
im.run()
print(im.get_modules())
#
# For the multilayer hyperedge-similarity representation, replace
# --representation bipartite with --representation multilayer --similarity
# The output is a *-multilayer.net file; pass --multilayer to Infomap.
```

The key modelling choices and their effects (Eriksson et al. 2021, Table 1):

| Walk model | Representation | Best for |
|---|---|---|
| Lazy | Unipartite | Fast exploration; standard pairwise tools apply |
| Lazy / non-lazy | Bipartite | Compact; explicit hyperedge assignments; fewest links |
| Non-lazy | Multilayer | Overlapping communities; persistent within-hyperedge flow |
| Hyperedge-similarity | Multilayer | Topically coherent communities; deepest nesting |

A practical rule of thumb from the paper: if you do not need overlapping
communities or explicit hyperedge assignments, the **unipartite clique
expansion** (with lazy walks) is a fast baseline. If you want explicit
hyperedge assignments or need to scale to large datasets, the **bipartite**
representation offers the smallest network and fastest Infomap runtime. The
**multilayer** representations give the richest structure but at much higher
computational cost: the fossil-record experiment (13 276 nodes, 77
hyperedges) took over nine hours for the multilayer case against six seconds
for the bipartite case.

## API pointers

- {attr}`infomap.Infomap.bipartite_start_id` gets or sets the node id at which
  the second node type (hyperedge-nodes) begins. Set it before `run()`. The
  setter `setBipartiteStartId(start_id)` does the same.
- {meth}`infomap.Infomap.add_link` adds a weighted or unweighted directed edge;
  use it for every `(node, hyperedge_node)` membership pair.
- {meth}`infomap.Infomap.get_modules` returns a `{node_id: module_id}` dict
  covering all nodes, including hyperedge-nodes.
- {attr}`infomap.Infomap.codelength` is the map equation value after the run; the
  flow of hyperedge-nodes is folded back into regular-node flow before
  evaluation.
- {attr}`infomap.Infomap.num_top_modules` is the number of top-level modules in
  the optimal partition.

## Going deeper

- {cite:t}`smiljanic2026survey`, §5.4, covers higher-order representations
  including hypergraphs.
- Companion notebook: `examples/notebooks/5.4 Modeling Multi-body Interactions.ipynb`
  walks through real hypergraph datasets with the mapping-hypergraphs tool.
- {cite:t}`eriksson2021hypergraph` shows how the representation choice shapes the
  communities found.
- {cite:t}`eriksson2022flow` compares the map equation with Markov stability on
  hypergraphs.
- The `mapping-hypergraphs` tool (Rust, MapEquation team):
  <https://github.com/mapequation/mapping-hypergraphs>
