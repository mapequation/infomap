"""Hypothesis strategies for small, valid networks of every input kind (#1252).

Each strategy draws a :class:`GeneratedNetwork`: the Pajek text Infomap reads,
the options that input needs, and what a run on it must return. The kinds:

``plain_networks``
    ``*Vertices`` with node weights and ``*Links`` (directed) or ``*Edges``
    (undirected), self-links and isolated nodes included.
``bipartite_networks``
    Primary nodes, then feature nodes from the ``*Bipartite`` start id, and
    links only between the two sides.
``multilayer_networks``
    ``*Intra``/``*Inter`` links, where some layers may have no inter links, or
    general ``*Multilayer`` links between any two layers.
``state_networks``
    ``*States`` over the physical nodes, at least one physical node with more
    than one state, and ``*Links`` between states.
``meta_networks``
    A plain or state network and a ``.meta`` file with a category for each
    node (state node in a state network), or for only some of them.

Node ids are drawn from a range wider than the node count, so they need not be
contiguous. Weights are positive and written with ``repr``, so the file holds
the drawn float exactly. The sizes stay small, so a run takes milliseconds and
a failing example shrinks to something readable.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from hypothesis import assume
from hypothesis import strategies as st

MAX_NODES = 10
MAX_ID = 40

weights = st.floats(min_value=0.01, max_value=100.0, allow_nan=False).map(
    lambda w: float(f"{w:.6g}")
)


@dataclass(frozen=True)
class GeneratedNetwork:
    kind: str
    text: str
    # Physical node ids a run must assign to a module.
    nodes: frozenset[int]
    # State ids a run must assign, for multilayer and state networks.
    state_nodes: int = 0
    directed: bool = False
    # Meta category per node id (state id in a state network).
    meta: dict[int, int] | None = None
    options: dict[str, Any] = field(default_factory=dict)

    @property
    def has_states(self) -> bool:
        return self.kind in ("multilayer", "state")

    def write(self, directory: Path) -> tuple[Path, dict[str, Any]]:
        """Write the input to ``directory``; return its path and the run options."""
        path = directory / f"{self.kind}.net"
        path.write_text(self.text)
        options = dict(self.options)
        if self.directed:
            options["directed"] = True
        if self.meta is not None:
            meta_path = directory / f"{self.kind}.meta"
            meta_path.write_text(
                "".join(f"{node} {category}\n" for node, category in self.meta.items())
            )
            options["meta_data"] = str(meta_path)
        return path, options

    def __repr__(self) -> str:
        # Hypothesis prints the falsifying example with repr; the file is what
        # reproduces it.
        meta = "" if self.meta is None else f"\n# meta: {self.meta}"
        flags = f" directed={self.directed}" if self.directed else ""
        return f"<{self.kind}{flags}{meta}\n{self.text}>"


def _number(x: float) -> str:
    return repr(x) if x != int(x) else str(int(x))


def _pajek(vertices: dict[int, float | None], sections) -> str:
    lines = [f"*Vertices {len(vertices)}"]
    for node, weight in vertices.items():
        lines.append(
            f'{node} "n{node}"' + ("" if weight is None else f" {_number(weight)}")
        )
    for header, rows in sections:
        lines.append(header)
        lines.extend(
            " ".join(_number(x) if isinstance(x, float) else str(x) for x in row)
            for row in rows
        )
    return "\n".join(lines) + "\n"


def _node_ids(draw, min_size: int, max_size: int, low: int = 1) -> list[int]:
    return sorted(
        draw(
            st.lists(
                st.integers(low, low + MAX_ID - 1),
                min_size=min_size,
                max_size=max_size,
                unique=True,
            )
        )
    )


def _node_weights(draw, nodes) -> dict[int, float | None]:
    with_weights = draw(st.booleans())
    return {node: draw(weights) if with_weights else None for node in nodes}


def _links(draw, sources, targets, *, self_links: bool, max_size: int):
    pairs = [(u, v) for u in sources for v in targets if self_links or u != v]
    if not pairs:
        return []
    chosen = draw(st.lists(st.sampled_from(pairs), max_size=max_size, unique=True))
    return [(u, v, draw(weights)) for u, v in chosen]


@st.composite
def plain_networks(draw, *, directed: bool | None = None, max_nodes: int = MAX_NODES):
    """A first-order network with weighted links, directed or undirected."""
    directed = draw(st.booleans()) if directed is None else directed
    nodes = _node_ids(draw, 1, max_nodes)
    links = _links(draw, nodes, nodes, self_links=True, max_size=3 * len(nodes))
    text = _pajek(
        _node_weights(draw, nodes),
        [("*Links" if directed else "*Edges", links)],
    )
    return GeneratedNetwork("plain", text, frozenset(nodes), directed=directed)


@st.composite
def bipartite_networks(draw, max_nodes: int = MAX_NODES):
    """Primary nodes below the ``*Bipartite`` start id, feature nodes from it."""
    directed = draw(st.booleans())
    num_primary = draw(st.integers(1, max_nodes - 1))
    num_features = draw(st.integers(1, max_nodes - num_primary))
    primary = list(range(1, num_primary + 1))
    features = list(range(num_primary + 1, num_primary + num_features + 1))
    links = _links(draw, primary, features, self_links=False, max_size=3 * max_nodes)
    # Links may be given from either side.
    links = [(v, u, w) if draw(st.booleans()) else (u, v, w) for u, v, w in links]
    text = _pajek(
        _node_weights(draw, primary + features),
        [(f"*Bipartite {features[0]}", links)],
    )
    return GeneratedNetwork(
        "bipartite", text, frozenset(primary + features), directed=directed
    )


@st.composite
def multilayer_networks(draw, max_nodes: int = 6, max_layers: int = 3):
    """``*Intra``/``*Inter`` links or general ``*Multilayer`` links.

    In the ``*Intra``/``*Inter`` form, the layers with inter links are drawn,
    so some layers (or all) may have none.
    """
    directed = draw(st.booleans())
    nodes = _node_ids(draw, 1, max_nodes)
    layers = list(range(1, draw(st.integers(1, max_layers)) + 1))
    general = draw(st.booleans())
    if general:
        layer_nodes = [(layer, node) for layer in layers for node in nodes]
        rows = [
            (a[0], a[1], b[0], b[1], w)
            for a, b, w in _links(
                draw, layer_nodes, layer_nodes, self_links=False, max_size=4 * max_nodes
            )
        ]
        sections = [("*Multilayer", rows)]
    else:
        intra = []
        for layer in layers:
            intra += [
                (layer, u, v, w)
                for u, v, w in _links(
                    draw, nodes, nodes, self_links=False, max_size=2 * len(nodes)
                )
            ]
        with_inter = draw(st.lists(st.sampled_from(layers), unique=True))
        inter = [
            (layer, node, other, w)
            for layer in with_inter
            for node, other, w in _links(
                draw,
                nodes,
                [x for x in layers if x != layer],
                self_links=True,
                max_size=len(nodes),
            )
        ]
        sections = [("*Intra", intra), ("*Inter", inter)]
    text = _pajek(dict.fromkeys(nodes), sections)
    # Only the layer nodes that links reach become state nodes, so a physical
    # node without links is not in the network. An inter link creates its
    # source layer node, and steps to the intra neighbours of the target
    # layer node, so it creates no node in the target layer.
    rows = sections[0][1]
    # Without intra links, inter links have nothing to step to, and the input
    # is read as a network of the vertices alone.
    assume(rows)
    linked = {row[1] for row in rows} | {row[3 if general else 2] for row in rows}
    if not general:
        linked |= {row[1] for row in sections[1][1]}
    return GeneratedNetwork("multilayer", text, frozenset(linked), directed=directed)


@st.composite
def state_networks(draw, max_nodes: int = 6, max_states: int = 10):
    """States over physical nodes, with at least one node holding two states."""
    directed = draw(st.booleans())
    nodes = _node_ids(draw, 1, max_nodes)
    num_states = draw(st.integers(len(nodes) + 1, max(len(nodes) + 1, max_states)))
    # Every physical node gets a state; the rest go to drawn nodes.
    physical = nodes + [
        draw(st.sampled_from(nodes)) for _ in range(num_states - len(nodes))
    ]
    physical = draw(st.permutations(physical))
    states = list(range(1, num_states + 1))
    with_weights = draw(st.booleans())
    state_rows = [
        (s, n, f'"s{s}"', *((draw(weights),) if with_weights else ()))
        for s, n in zip(states, physical, strict=True)
    ]
    links = _links(draw, states, states, self_links=True, max_size=3 * num_states)
    text = _pajek(dict.fromkeys(nodes), [("*States", state_rows), ("*Links", links)])
    return GeneratedNetwork(
        "state", text, frozenset(nodes), state_nodes=num_states, directed=directed
    )


@st.composite
def meta_networks(draw):
    """A plain or state network with meta categories on all or some nodes."""
    net = draw(st.one_of(plain_networks(), state_networks()))
    ids = (
        list(range(1, net.state_nodes + 1))
        if net.kind == "state"
        else sorted(net.nodes)
    )
    tagged = draw(
        st.lists(st.sampled_from(ids), min_size=1, unique=True)
        if draw(st.booleans())
        else st.just(ids)
    )
    num_categories = draw(st.integers(1, 3))
    meta = {node: draw(st.integers(1, num_categories)) for node in sorted(tagged)}
    return GeneratedNetwork(
        net.kind,
        net.text,
        net.nodes,
        state_nodes=net.state_nodes,
        directed=net.directed,
        meta=meta,
    )


KINDS = {
    "plain": plain_networks,
    "bipartite": bipartite_networks,
    "multilayer": multilayer_networks,
    "state": state_networks,
    "meta": meta_networks,
}
