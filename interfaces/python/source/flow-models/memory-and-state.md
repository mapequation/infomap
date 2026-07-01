---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Memory and state networks

{bdg-warning-line}`Flow model`

```{admonition} In one sentence
:class: tip
When where flow goes next depends on where it came from, you need a *memory
network*: state nodes encode the context, and the map equation on state nodes
reveals overlapping communities that a first-order model cannot see.
```

## When flow remembers where it came from

Standard community detection treats a network as a memoryless system. At every
node, a random walker continues to the next node proportional to link weights,
and nothing about the history of the walk matters. This first-order Markov
assumption is often a reasonable starting point, but in many real systems the
dynamics carry memory: where flow goes next depends on where it has been.

Consider air passengers. A traveller arriving at Chicago from Seattle behaves
differently from one arriving from New York: the Seattle traveller tends to
return to Seattle, the New York traveller to New York. A standard network model,
which sees only "a traveller at Chicago," cannot capture that distinction.
Researchers studying hospital patient transfers, citation patterns, and email
forwarding chains have found the same memory effect, where the path already
taken constrains the next step {cite:p}`rosvall2014memory`.

Ignoring this memory distorts community structure in two related ways. First,
it makes modules look larger and less specific, because flow that is actually
channelled by history appears to leak freely across boundaries. Second, and more
striking, it hides *overlapping* communities. In a second-order model, a
multidisciplinary journal such as *PNAS* belongs simultaneously to several
research communities depending on where a citation arrives from. In a
first-order model it lands in a single module. Memory networks restore
the overlap.

## Splitting a node by its history

The core idea is to split each physical location into multiple *state nodes*,
one for each relevant piece of context. In a second-order model the context is
the previously visited node, so each directed edge $i \to j$ in the original
network becomes a state node at $j$ that represents the history "arrived from
$i$."

Think of a city intersection where two different streams of traffic pass
through. One stream enters from the west and exits to the east, passing straight
through. Another enters from the south and exits back to the south, a local
detour. Model the intersection at first order and it appears in one community.
Model the actual pathways and the two streams belong to different communities,
with the intersection *overlapping* between them.

Infomap handles this with the **physical node / state node** distinction (see
{doc}`/concepts/state-nodes-and-higher-order-flow` for the general mechanism).
State nodes carry the dynamics: they have links and stationary visit rates.
Physical nodes represent the real objects you care about: they aggregate all
their state nodes. The map equation encodes flow at the state-node level but
names physical nodes in its codebook, so it correctly records a physical node
whose state nodes land in different modules as *overlapping* {cite:p}`edler2017higher`.

## Memory as a state-node network

A standard first-order network has physical nodes $i$ and transition
probabilities $P_{ij} = w_{ij}/w_i$. In a second-order model, the walker's
next step depends on both the current node $j$ and the previously visited node
$i$. We encode this as a state node $\overrightarrow{ij}$ at physical node $j$.
The transition probabilities become

$$
P\!\left(\overrightarrow{ij} \to \overrightarrow{jk}\right)
= \frac{w_{\overrightarrow{ij} \to \overrightarrow{jk}}}
       {\sum_l w_{\overrightarrow{ij} \to \overrightarrow{jl}}}
$$

This is a first-order Markov chain on the expanded state-node graph, so
Infomap's standard machinery applies unchanged. The stationary visit rate
of physical node $k$ is the sum over all its state nodes:

$$
\pi_k = \sum_{\overrightarrow{jk}} \pi_{\overrightarrow{jk}}
$$

When multiple state nodes of the same physical node land in the same module,
the map equation aggregates their visit rates into a single codebook entry for
that physical node. A physical node whose state nodes are *split across
modules* is therefore assigned to multiple modules; overlapping community
membership arises from the state-node formalism {cite:p}`edler2017higher`.

You do not have to commit to a full second-order model. The *sparse memory
network* framework {cite:p}`persson2016sparse` lets you lump state nodes with
similar outlink distributions, by minimising the entropy-rate loss, producing a
variable-order model that fits the data efficiently without the exponential
blowup of a fixed higher-order model.

:::{toggle}
**State-node map equation**

For a partition $\mathsf{M}$ of state nodes into modules, the two-level map
equation is

$$
L(\mathsf{M}) = q_{\curvearrowright} H(\mathcal{Q})
+ \sum_{m=1}^{M} p^{\circlearrowright}_m H(\mathcal{P}_m)
$$

The within-module codebook entropy $H(\mathcal{P}_m)$ ranges over
physical nodes, not state nodes:

$$
H(\mathcal{P}_m) =
-\frac{q_{m\curvearrowright}}{p^{\circlearrowright}_m}
  \log\frac{q_{m\curvearrowright}}{p^{\circlearrowright}_m}
-\sum_{i \in \mathsf{M}_m}
  \frac{\pi_{i \cap m}}{p^{\circlearrowright}_m}
  \log\frac{\pi_{i \cap m}}{p^{\circlearrowright}_m}
$$

where $\pi_{i \cap m} = \sum_{\alpha_i \in \mathsf{M}_m} \pi_{\alpha_i}$
aggregates all state nodes of physical node $i$ that are assigned to module
$m$. This aggregation is what enables overlapping physical-node membership:
state nodes of the same physical node in different modules each contribute to
separate codebook entries; see {cite:p}`edler2017higher`, §3.2.

The exit rate from module $m$ is

$$
q_{m\curvearrowright} =
\sum_{\substack{\alpha_i \in \mathsf{M}_m \\ \beta_j \notin \mathsf{M}_m}}
\pi_{\alpha_i} P_{\alpha_i \beta_j}
$$

These expressions reduce to the standard first-order map equation when each
physical node has exactly one state node {cite:p}`rosvall2008maps`.
:::

## A junction shared by two flows

The example below constructs a small four-node network where two independent
streams of flow pass through a shared junction node **B**:

- **Stream 1:** $A \to B \to C \to A$ (a tight triangle)
- **Stream 2:** $D \to B \to D$ (an out-and-back trip)

In a first-order view, node B sits at the intersection and ends up lumped with
whichever stream has more traffic. In a second-order view, the two streams
through B travel on different state nodes, so Infomap assigns B to *both*
modules at once.

### Step 1: First-order baseline

Build the aggregated physical network and run Infomap with first-order flow.

```{code-cell} python
import networkx as nx
import infomap
from infomap import Network, run
from docs_viz import draw_partition

# Directed graph: two streams share junction B (node 1)
# Stream 1: A(0)→B(1)→C(2)→A(0)
# Stream 2: D(3)→B(1)→D(3)
g = nx.DiGraph()
g.add_nodes_from([0, 1, 2, 3])
g.add_edges_from([(0, 1), (1, 2), (2, 0),   # stream 1
                  (3, 1), (1, 3)])            # stream 2

node_name = {0: "A", 1: "B", 2: "C", 3: "D"}

result1 = infomap.run(g, two_level=True, silent=True)
modules1 = result1.modules()   # {node_id: module_id}

print("First-order: node → module")
for nid, mod in sorted(modules1.items()):
    print(f"  {node_name[nid]} → module {mod}")
print(f"Distinct modules: {len(set(modules1.values()))}")
```

With first-order flow the walker mixes the two streams at B, and all four
nodes collapse into a single module. The junction obscures the boundary.

### Step 2: Second-order state-node network

Now build the same network as a state-node graph. Each physical node gets one
state node per incoming stream. Node B gets **two** state nodes:

- state 10 (physical 1): flow that arrived from stream 1
- state 11 (physical 1): flow that arrived from stream 2

```{code-cell} python
net = Network()

# Declare state nodes: add_state_node(state_id, physical_id)
net.add_state_node(0,  0)   # phys A, state 0
net.add_state_node(2,  2)   # phys C, state 2
net.add_state_node(3,  3)   # phys D, state 3
net.add_state_node(10, 1)   # phys B, state 10 (stream 1 context)
net.add_state_node(11, 1)   # phys B, state 11 (stream 2 context)

# Add directed links between STATE nodes
# Stream 1: A(0) → B_s1(10) → C(2) → A(0)
net.add_link(0,  10, 1.0)
net.add_link(10, 2,  1.0)
net.add_link(2,  0,  1.0)

# Stream 2: D(3) → B_s2(11) → D(3)
net.add_link(3,  11, 1.0)
net.add_link(11, 3,  1.0)

result2 = run(net, two_level=True, silent=True)

n_physical = len({node.node_id for node in result2.nodes(states=True)})
print(f"have_memory: {result2.have_memory}")
print(f"physical nodes: {n_physical}")
print()

# For memory networks, modules() requires states=True
state_modules = result2.modules(states=True)   # {state_id: module_id}

print("State node assignments:")
phys_label = {0: "A", 2: "C", 3: "D", 10: "B (stream-1 ctx)", 11: "B (stream-2 ctx)"}
for sid, mod in sorted(state_modules.items()):
    print(f"  state {sid:>2} ({phys_label.get(sid, '?')}) → module {mod}")
```

### Step 3: Read overlapping physical-node membership

Iterate `result.nodes(states=True)` to see which physical node appears in more
than one module. Each state node exposes `.node_id` (the physical node it
belongs to), `.state_id`, and `.module_id`.

```{code-cell} python
from collections import defaultdict

phys_to_modules = defaultdict(list)
for node in result2.nodes(states=True):
    phys_to_modules[node.node_id].append(node.module_id)

print("Physical node membership after second-order analysis:")
for phys_id, mods in sorted(phys_to_modules.items()):
    overlap = " ← OVERLAP" if len(mods) > 1 else ""
    print(f"  {node_name[phys_id]} (phys {phys_id}): modules {mods}{overlap}")
```

Node B appears in **both** module 1 and module 2; it bridges the two streams,
correctly identified as overlapping.

### Step 4: Visualise

We colour the physical graph by each node's *primary* module (the first module
it belongs to). The visualisation shows the two communities; the caption reminds
you that B is shared.

```{code-cell} python
import matplotlib.pyplot as plt
from myst_nb import glue

# Primary module per physical node (first listed)
primary = {pid: mods[0] for pid, mods in phys_to_modules.items()}

# Physical-node flow: sum the flow of every state node sharing that physical id.
flow = {}
for node in result2.nodes(states=True):
    flow[node.node_id] = flow.get(node.node_id, 0.0) + node.flow

fig = draw_partition(g, primary, flow=flow)
fig.axes[0].set_title(
    "Second-order: B overlaps modules 1 and 2\n"
    "(colour shows primary assignment)",
    fontsize=9,
)
glue("fig-memory-and-state", fig, display=False)
plt.close(fig)
```

```{glue:figure} fig-memory-and-state
The physical nodes coloured by their primary module. Node B sits on the
boundary: with second-order (state-node) flow it belongs to both
communities, so a single colour can show only one of its two memberships.
```

### Contrast

| Node | First-order | Second-order |
|------|-------------|--------------|
| A    | one shared module | stream 1 |
| B    | one shared module | streams 1 **and** 2 (overlap) |
| C    | one shared module | stream 1 |
| D    | one shared module | stream 2 |

The first-order model sees one community of four nodes. The second-order model
finds two communities (A–B–C and B–D) that properly overlap at junction B.

## API pointers

**Declaring state nodes**

```python
net = Network()
net.add_state_node(state_id, physical_id)
```

Creates a state node with a unique integer `state_id` mapped to `physical_id`.
If the physical node does not yet exist, `add_state_node` creates it. Use
{meth}`infomap.Network.set_name` afterwards for a human-readable label.

**Adding state-level links**

```python
net.add_link(source_state_id, target_state_id, weight=1.0)
```

Links connect *state nodes*, not physical nodes. The same `add_link` serves both
first-order and higher-order networks; in a state-node network the integer ids
refer to state node ids.

**Querying results**

```python
result = run(net)
result.have_memory                            # True when state nodes were added

# For memory networks, pass states=True
state_modules = result.modules(states=True)   # {state_id: module_id}

# Group state nodes by their physical node to find overlap
for node in result.nodes(states=True):
    node.node_id    # physical node id
    node.state_id   # state node id
    node.module_id  # module assignment for this state node
    node.flow       # state-node visit frequency
```

Because a physical node has one entry per state node in
`result.nodes(states=True)`, collecting all its module assignments requires
grouping by `node_id`, as shown in the worked example.

## Going deeper

- {cite:t}`smiljanic2026survey`, §5.1, covers higher-order and memory flows.
- Companion notebook: `examples/notebooks/5.1 Memory Networks.ipynb` works a
  full-size example with empirical pathway data and sparse-Markov-chain model
  selection.
- Source paper for memory networks {cite:p}`rosvall2014memory`.
- Higher-order flows with Infomap {cite:p}`edler2017higher`.
- {cite:t}`persson2016sparse` introduces sparse Markov chains for memory flows.
