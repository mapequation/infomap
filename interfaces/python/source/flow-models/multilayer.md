---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Multilayer networks

{bdg-warning-line}`Flow model`

```{admonition} In one sentence
:class: tip
A multilayer network lets the same physical node live in several layers at once,
so Infomap can discover that a person belongs to one community at work and a
different one at home, without collapsing those contexts into one network.
```

## When one network flattens the picture

Many real systems involve the same actors interacting through *several kinds of
relationship at once*. Scientists collaborate on journal papers, present at
conferences, and correspond by email. Passengers fly between airports on
different airlines. People message each other through different apps. Collapse
all these interaction types into one network and you lose the contextual
information that defines who belongs to which community in each setting.

A **multilayer network** preserves those contexts by keeping each interaction
type in its own layer. The same physical entity (a scientist, an airport, a
person) exists in each layer where it participates, but its neighbourhood and
community membership can differ from layer to layer. Aggregating the layers into
one network can distort both the topology and the community structure {cite:p}`domenico2015multilayer`.

The payoff is **overlapping community detection without an extra algorithm**.
Because each layer contributes state nodes for the same physical nodes, the map
equation can assign a physical node to several modules, one per layer where its
local structure places it. You get non-overlapping partitions within each layer
and physically meaningful overlap across them.

## One node, several layers

Think of a research collaboration network. Layer 1 records co-authorship on
machine-learning papers; Layer 2 records co-authorship on statistics papers.
Researcher Alice publishes in both fields. Her Layer-1 state node connects to her
ML co-authors, her Layer-2 state node to her statistics co-authors. Those are two
different neighbourhoods.

Infomap runs a random walk across all the state nodes. The walk stays in its
current layer most of the time and crosses to another at rate $r$, the **relax
rate**. When it rarely crosses ($r$ small), each layer behaves on its own and
Alice lands in a different module in each of her layers. When it crosses freely
($r = 1$), the layers fuse and Alice gets one module. The default $r = 0.15$
switches layers about once in six steps, enough coupling to respect the
multiplex structure without washing out the per-layer signal.

The key step is the **physical node / state node** distinction
({doc}`/concepts/state-nodes-and-higher-order-flow`): here a state node is a
physical node's presence in one layer. The map equation tracks state nodes for
the walk but gives state nodes of the same physical node a shared codeword within
a module. So a physical node that lands in two modules is
bi-modular, with a flow signature that differs by layer {cite:p}`edler2017higher`.

## State nodes and the relax rate

A multilayer network has $L$ layers over a shared set of physical nodes. Each
physical node $i$ in layer $\alpha$ becomes a **state node** $(i, \alpha)$.
Links within a layer (intra-layer) connect state nodes in the same layer;
links across layers (inter-layer) connect state nodes in different layers.

Without empirical inter-layer link weights, Infomap models inter-layer movement
with a relax rate $r \in [0, 1]$. At each step, the random walker follows
intra-layer links with probability $1 - r$ and relaxes the layer constraint
with probability $r$, freely following any link of the physical node across all
layers:

$$
\mathcal{P}_{ij}^{\alpha\beta}(r) =
  (1 - r)\,\delta_{\alpha\beta}\,\frac{w_{ij}^{\beta}}{s_i^{\beta}}
  + r\,\frac{w_{ij}^{\beta}}{S_i}
$$

where $s_i^{\beta}$ is the intra-layer out-strength of node $i$ in layer
$\beta$, and $S_i = \sum_\beta s_i^\beta$ is the total out-strength across all
layers. Setting $r = 0$ decouples layers completely; $r = 1$ is equivalent to
running Infomap on the aggregated single-layer network (but still allowing
overlap).

The map equation then encodes the random walker's trajectory through these state
nodes. If two state nodes $(i, \alpha)$ and $(i, \beta)$ land in the
same module, they share a codeword, because they represent the same physical
object. If they land in different modules, physical node $i$ is bi-modular, a
member of two overlapping communities.

:::{toggle}
**State node transition probabilities (full derivation)**

A multilayer network is a set of state nodes $\{(i, \alpha)\}$ with intra-layer
link weights $w_{ij}^\alpha$ (both $i$ and $j$ in layer $\alpha$) and
inter-layer link weights $D_i^{\alpha\beta}$ (physical node $i$ from layer
$\alpha$ to layer $\beta$). The general transition probability is

$$
\mathcal{P}_{ij}^{\alpha\beta}
= \frac{D_i^{\alpha\beta}}{S_i^{\alpha}}\,\frac{w_{ij}^{\beta}}{s_i^{\beta}},
$$

where $S_i^\alpha = \sum_\beta D_i^{\alpha\beta}$ is the inter-layer
out-strength of node $i$ from layer $\alpha$.

When no empirical inter-layer data are available, setting
$D_i^{\alpha\beta} = (1-r)\delta_{\alpha\beta}S_i + r\,s_i^\beta$ recovers the
relax-rate formula above {cite:p}`domenico2015multilayer`. At $r = 0.15$ the
walker stays in a layer for about $1/r \approx 6$ steps before switching, enough
coupling for layers to inform each other without fusing.

The map equation then minimises

$$
L(\mathsf{M}) =
  q_{\curvearrowright} H(\mathcal{Q})
  + \sum_{\boldsymbol{\imath}=1}^{m}
      p_{\boldsymbol{\imath}\circlearrowright}
      H(\mathcal{P}^{\boldsymbol{\imath}})
$$

over partitions of state nodes into modules, where the map equation sums the
visit rates of the same physical node in the same module before computing the
module codebook entropy $H(\mathcal{P}^{\boldsymbol{\imath}})$. This is the only
difference from the standard first-order map equation, and it is precisely what
makes physical nodes naturally bi-modular {cite:p}`domenico2015multilayer,edler2017higher`.
:::

## Two triangles bridged by one node

The network below has **five physical nodes** and two layers.

- **Layer 1** contains a tight triangle: nodes 1–2–3.
- **Layer 2** contains a tight triangle: nodes 1–4–5.

Physical node **1** is the bridge: it participates in both layers. With a low
relax rate, the map equation finds two modules, {1,2,3} from Layer 1 and {1,4,5}
from Layer 2. Node 1 is bi-modular: module 1 in Layer 1, module 2 in Layer 2.

```{code-cell} python
import infomap
from infomap import Network, run

net = Network()

# Layer 1: tight triangle {1, 2, 3}
for src, tgt in [(1, 2), (2, 3), (1, 3)]:
    net.add_multilayer_intra_link(1, src, tgt, weight=1.0)
    net.add_multilayer_intra_link(1, tgt, src, weight=1.0)

# Layer 2: tight triangle {1, 4, 5}
for src, tgt in [(1, 4), (4, 5), (1, 5)]:
    net.add_multilayer_intra_link(2, src, tgt, weight=1.0)
    net.add_multilayer_intra_link(2, tgt, src, weight=1.0)

result = run(net, multilayer_relax_rate=0.15, seed=123, num_trials=10, silent=True)

state_nodes = list(result.nodes(states=True))
print(f"Modules found : {result.num_top_modules}")
print(f"Codelength    : {result.codelength:.4f} bits")
print(f"Physical nodes: {len({n.node_id for n in state_nodes})}")
print(f"State nodes   : {len(state_nodes)}")
```

Infomap operates on six state nodes (one per physical-node–layer pair) and
finds two modules. Now inspect the per-state-node partition to see the overlap:

```{code-cell} python
print(f"{'Layer':>6}  {'Phys node':>10}  {'Module':>8}")
print("-" * 30)
for node in sorted(result.nodes(states=True), key=lambda n: (n.layer_id, n.node_id)):
    print(f"{node.layer_id:>6}  {node.node_id:>10}  {node.module_id:>8}")
```

Physical node **1** appears twice, once in each layer, in a different module each
time. Nodes 2 and 3 sit only in module 1 (the Layer 1 triangle), nodes 4 and 5
only in module 2 (the Layer 2 triangle).

On multilayer networks you must call `result.modules(states=True)`, because each
physical node can belong to multiple modules:

```{code-cell} python
state_modules = result.modules(states=True)
print("state_id -> module_id:", state_modules)
```

You can recover the full per-layer picture from `result.nodes(states=True)`:

```{code-cell} python
from collections import defaultdict

# physical node -> list of (layer, module) pairs
phys_memberships = defaultdict(list)
for node in result.nodes(states=True):
    phys_memberships[node.node_id].append((node.layer_id, node.module_id))

print(f"{'Phys node':>10}  {'(layer, module) assignments'}")
print("-" * 45)
for pid, assignments in sorted(phys_memberships.items()):
    print(f"{pid:>10}  {sorted(assignments)}")
```

Physical node 1 has two assignments; all others have one. That is the
multilayer overlap in action.

Now draw both layers side by side, colouring each node by the module it lands
in. A shared palette keeps a module the same colour across panels, so physical
node 1 shows up in a different colour in each layer: that colour change *is* the
bi-modular overlap made visible.

```{code-cell} python
from myst_nb import glue

import matplotlib.pyplot as plt
import networkx as nx
from docs_viz import draw_partition, module_palette

layers = {
    1: ([(1, 2), (2, 3), (1, 3)], "Layer 1"),
    2: ([(1, 4), (4, 5), (1, 5)], "Layer 2"),
}

# Module assignment and flow per physical node, recorded separately per layer.
state_nodes = list(result.nodes(states=True))
module_in_layer = {
    layer: {n.node_id: n.module_id for n in state_nodes if n.layer_id == layer}
    for layer in (1, 2)
}
flow_in_layer = {
    layer: {n.node_id: n.flow for n in state_nodes if n.layer_id == layer}
    for layer in (1, 2)
}
colours = module_palette(
    m for layer in (1, 2) for m in module_in_layer[layer].values()
)

fig, axes = plt.subplots(1, 2, figsize=(8, 4), layout="constrained")
for ax, (layer, (edges, title)) in zip(axes, layers.items()):
    G = nx.Graph()
    G.add_edges_from(edges)
    draw_partition(
        G, module_in_layer[layer], ax=ax, module_colors=colours,
        with_labels=True, node_size=600, flow=flow_in_layer[layer],
    )
    ax.set_title(title, fontsize=11)

fig.suptitle("Physical node 1 is bi-modular: one colour per layer", fontsize=12)
glue("fig-multilayer", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-multilayer
The two layers drawn separately, coloured by module with a shared palette.
Physical node 1 appears in both layers in a different colour: the bi-modular
overlap that state nodes make possible.
```

The three Layer-1 nodes share one colour: they form a single module. In Layer 2
the same physical node 1 takes a different colour, because it belongs to the
other cluster in that context.

### Relax rate sensitivity

The relax rate controls how strongly the layers couple. Low values treat each
layer nearly independently; high values merge them into a single partition.

```{code-cell} python
results = []
for r in [0.01, 0.15, 0.50, 0.90, 1.00]:
    net_r = Network()
    for src, tgt in [(1, 2), (2, 3), (1, 3)]:
        net_r.add_multilayer_intra_link(1, src, tgt, 1.0)
        net_r.add_multilayer_intra_link(1, tgt, src, 1.0)
    for src, tgt in [(1, 4), (4, 5), (1, 5)]:
        net_r.add_multilayer_intra_link(2, src, tgt, 1.0)
        net_r.add_multilayer_intra_link(2, tgt, src, 1.0)
    result_r = run(net_r, multilayer_relax_rate=r, seed=123, num_trials=10, silent=True)
    results.append((r, result_r.num_top_modules, result_r.codelength))

print(f"{'relax rate':>12}  {'modules':>8}  {'codelength':>12}")
print("-" * 36)
for r, m, cl in results:
    print(f"{r:>12.2f}  {m:>8}  {cl:>12.4f}")
```

At low relax rates the two triangle clusters remain distinct (2 modules). As
$r \to 1$ the layers fuse and the partition collapses to 1 module, the same as
running Infomap on the aggregated network. The default $r = 0.15$ is the working
value from {cite:t}`domenico2015multilayer`, who found the partition only weakly
dependent on the relax rate; robustness holds across a wide range of empirical
multilayer networks at around $r \approx 0.25$ {cite:p}`edler2017higher`.

### Using explicit inter-layer links

When your data include observed inter-layer transitions (for example, passenger
bookings that start on one airline and continue on another), you can supply them
directly with `add_multilayer_inter_link` instead of relying on the relax-rate
model. The inter-layer link specifies a coupling weight through a shared physical
node:

```{code-cell} python
net_inter = Network()

# Intra-layer links (same as before)
for src, tgt in [(1, 2), (2, 3), (1, 3)]:
    net_inter.add_multilayer_intra_link(1, src, tgt, 1.0)
    net_inter.add_multilayer_intra_link(1, tgt, src, 1.0)
for src, tgt in [(1, 4), (4, 5), (1, 5)]:
    net_inter.add_multilayer_intra_link(2, src, tgt, 1.0)
    net_inter.add_multilayer_intra_link(2, tgt, src, 1.0)

# Explicit inter-layer coupling: node 1 bridges layer 1 <-> layer 2
net_inter.add_multilayer_inter_link(source_layer_id=1, node_id=1, target_layer_id=2, weight=0.15)
net_inter.add_multilayer_inter_link(source_layer_id=2, node_id=1, target_layer_id=1, weight=0.15)

result_inter = run(net_inter, seed=123, num_trials=10, silent=True)
print(f"Modules found : {result_inter.num_top_modules}")
print(f"Codelength    : {result_inter.codelength:.4f} bits")
```

When you provide explicit inter-layer links, Infomap does not use the relax-rate
model; the link weights you supply fully specify the coupling.

## API pointers

- {meth}`infomap.Network.add_multilayer_intra_link` adds a link within a layer:
  `add_multilayer_intra_link(layer_id, source_node_id, target_node_id,
  weight=1.0)`.
- {meth}`infomap.Network.add_multilayer_inter_link` adds a coupling between
  layers through a shared physical node:
  `add_multilayer_inter_link(source_layer_id, node_id, target_layer_id,
  weight=1.0)`.
- `multilayer_relax_rate` (default `0.15`) is an engine option on
  {func}`infomap.run`: the relax rate used when no explicit inter-layer links are
  provided.
- {meth}`infomap.Result.modules` needs `states=True` on multilayer networks to
  return state-node assignments; `states=False` raises a `RuntimeError` on
  higher-order networks.
- {meth}`infomap.Result.nodes` with `states=True` iterates state nodes; each
  exposes `.node_id` (physical), `.state_id`, `.layer_id`, `.module_id`, and
  `.flow`.

## Options

Multilayer flow is controlled by three engine options on {func}`infomap.run`:

| Option | Default | Effect |
|---|---|---|
| `multilayer_relax_rate` | `0.15` | Inter-layer coupling used when no explicit inter-layer links are given |
| `multilayer_relax_limit` | `-1` | Caps how many relax steps can occur in a row (`-1` for unlimited) |
| `multilayer_relax_to_self` | `False` | Restricts relaxed steps to the node's own neighbours in other layers (node-aligned coupling) |

## Going deeper

- {cite:t}`smiljanic2026survey`, §5.2, covers multilayer networks.
- Companion notebook: `examples/notebooks/5.2 Multilayer Networks.ipynb`
- {cite:t}`domenico2015multilayer` is the source paper for multilayer flows.
- {cite:t}`edler2017higher` maps higher-order flows with Infomap.
