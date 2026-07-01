---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# State nodes and higher-order flow

```{admonition} In one sentence
:class: tip
A *state node* lets one real-world node carry several flow contexts at once. It
is the single idea behind memory, multilayer, and temporal networks, and it is
what lets a node belong to more than one community.
```

## Motivation

The map equation in the previous chapters assumes **first-order** flow: where the
walker goes next depends only on where it is now. That is often too simple. Where
a traveller flies next depends on where they came *from*; which colleagues a
person interacts with depends on *which* setting (work, home, a conference); who
is connected depends on *when*. Collapsing that context into a plain network
averages it away, and the communities you get average it away too.

The fix is to stop treating a node as a single thing. This one mechanism, the
**state node**, underlies every chapter in *Flow models*: memory networks,
multilayer and multiplex networks, and temporal networks are all the same
construction with different notions of "context."

## Intuition

Separate two ideas that an ordinary network conflates:

- a **physical node** is the real-world object: an airport, a person, a paper;
- a **state node** is that object *in one context*: "Chicago, arrived from
  Seattle"; "Alice, at work"; "this paper, in 2021."

Infomap runs its random walk over the state nodes, and the map equation
partitions *those*. The only change from the first-order map equation is that
state nodes belonging to the **same physical node within the same module share a
codeword**, because they are the same object, so naming it costs once.

The payoff is **overlap for free**. If a physical node's state nodes land in
different modules, that physical node belongs to several communities at once,
with no separate overlapping-community algorithm. Alice-at-work and Alice-at-home
can sit in different groups; a hub airport can belong to several regional systems.

## Theory

A higher-order network is a set of state nodes $\{\alpha_i\}$ (state $i$ of
physical node $\alpha$) with transitions between them. The map equation is
unchanged in form,

$$
L(\mathsf{M}) =
  q_{\curvearrowright} H(\mathcal{Q})
  + \sum_{i=1}^{m} p_{\circlearrowright}^i H(\mathcal{P}^i),
$$

but it runs over state nodes: the map equation sums the visit rates of state
nodes of the same physical node in the same module before computing the module
codebook entropy. That summation is the whole difference, and it is what makes
physical nodes naturally multi-modular.

## A worked example

The smallest illustration: a physical node that two state nodes split between two
groups. Build state nodes directly with `add_state_node(state_id, physical_id)`
and link them, then read the partition back with `result.modules(states=True)`.

```{code-cell} python
import infomap
from infomap import Network, run

net = Network()

# Physical node 1 has two state nodes (10 and 11), one in each group.
# Group A: states 10,2,3 ; Group B: states 11,4,5. Physical 1 bridges them.
net.add_state_node(10, 1); net.add_state_node(11, 1)
for s in (2, 3, 4, 5):
    net.add_state_node(s, s)

for a, b in [(10, 2), (2, 3), (3, 10)]:          # triangle A
    net.add_link(a, b); net.add_link(b, a)
for a, b in [(11, 4), (4, 5), (5, 11)]:          # triangle B
    net.add_link(a, b); net.add_link(b, a)

result = run(net, two_level=True, seed=123, num_trials=10, silent=True)

print(f"Modules: {result.num_top_modules}")
phys_to_modules = {}
for node in result.nodes(states=True):
    phys_to_modules.setdefault(node.node_id, set()).add(node.module_id)
for pid, mods in sorted(phys_to_modules.items()):
    print(f"  physical node {pid}: module(s) {sorted(mods)}")
```

Physical node 1 appears in both modules; every other node sits in one. That
two-module membership is the overlap the state-node construction buys you.

Draw the two triangles, colouring each state node by the module Infomap
assigned it. State nodes 10 and 11 are physical node 1 in its two contexts;
they take different colours, and that colour split *is* the overlap.

```{code-cell} python
import matplotlib.pyplot as plt
import networkx as nx
from docs_viz import draw_partition
from myst_nb import glue

g = nx.Graph()
g.add_edges_from([(10, 2), (2, 3), (3, 10), (11, 4), (4, 5), (5, 11)])

# Colour each state node by its module; size it by its flow.
state_module = {n.state_id: n.module_id for n in result.nodes(states=True)}
state_flow = {n.state_id: n.flow for n in result.nodes(states=True)}

fig = draw_partition(g, state_module, with_labels=True, node_size=600, flow=state_flow)
glue("fig-state-nodes", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-state-nodes
The two triangles, coloured by module. State nodes 10 and 11 are physical node 1
in its two contexts; they land in different modules, so physical node 1 belongs
to both. Every other node sits in a single module.
```

## Where this goes next

Each *Flow models* chapter is this idea with a specific kind of context:

- {doc}`/flow-models/memory-and-state`: context is *where you came from*.
- {doc}`/flow-models/multilayer`: context is *which layer* (relationship type).
- {doc}`/flow-models/temporal`: context is *which time window*.

They build the state nodes for you through higher-level APIs
(`add_multilayer_intra_link`, time-window layers) rather than `add_state_node`
directly, but the partition you read back is always over state nodes.

## API pointers

- `Network().add_state_node(state_id, physical_id)` declares a state node;
  `add_link` then links state nodes.
- You need `result.modules(states=True)` to read a higher-order partition;
  `result.nodes(states=True)` exposes each node's `.node_id` (physical),
  `.state_id`, `.module_id`, and `.flow`.
- `result.have_memory` is `True` once a higher-order network is built.

## Going deeper

- {cite:t}`smiljanic2026survey`, §5, treats higher-order flow and state nodes in
  full.
- {cite:t}`edler2017higher` maps higher-order flows in memory and multilayer
  networks with Infomap.
