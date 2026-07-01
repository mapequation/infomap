"""Internal documentation figure helper.

One consistent style for every chapter: a module-colored network layout.
Not part of the public ``infomap`` package; it lives in the docs source
tree and is imported only by executed documentation pages and its test.
No alluvial diagrams; the hierarchy, temporal, and multilayer chapters reuse
this same layout (one panel per level, timestep, or layer) and pass a shared
``module_colors`` map so a module keeps its color across panels.
"""

from __future__ import annotations

import math

import matplotlib.pyplot as plt
import networkx as nx

__all__ = ["draw_partition", "module_palette"]


def module_palette(module_ids):
    """Return a stable ``{module_id: color}`` map for the given module ids.

    Module ids are sorted, then assigned colors from ``tab10`` (``tab20`` once
    there are more than ten modules). Pass the result to several
    :func:`draw_partition` calls so the same module keeps one color across the
    panels of a multi-panel figure.
    """
    ids = sorted(set(module_ids))
    cmap = plt.get_cmap("tab10" if len(ids) <= 10 else "tab20")
    return {m: cmap(i % cmap.N) for i, m in enumerate(ids)}


def draw_partition(
    graph,
    modules,
    *,
    seed=123,
    ax=None,
    node_size=120,
    with_labels=False,
    weight="weight",
    module_colors=None,
    flow=None,
):
    """Draw ``graph`` with nodes colored by their module.

    Parameters
    ----------
    graph : networkx.Graph
        The network to draw.
    modules : dict
        Mapping ``node -> module id``; must contain an entry for every node in
        ``graph``. Each distinct module id gets one color.
    seed : int
        Layout seed, so figures are reproducible across builds.
    ax : matplotlib.axes.Axes, optional
        Draw onto this axes; otherwise a new figure/axes is created. When an
        axes is passed the caller owns the layout (this function does not call
        ``tight_layout``), so a multi-panel figure can add a ``suptitle``
        without it colliding with the per-axes titles.
    node_size : int
        Base marker area in points squared (matplotlib scatter convention). With
        ``flow`` it is the area of the average-flow node; without it every node
        draws at this size.
    with_labels : bool
        Draw the node id inside each marker. Useful for the small graphs in the
        flow-model chapters; leave off for larger networks such as the karate
        club.
    weight : str
        Edge attribute read as the link's flow. Line width scales with its
        square root, so a node's marker radius and a link's width both grow as
        the square root of the flow they carry. Edges without it draw flat.
    module_colors : dict, optional
        Explicit ``{module id -> color}`` map. Pass the same map to every panel
        of a multi-panel figure so a module keeps one color throughout;
        otherwise a per-call palette is built from ``modules``.
    flow : dict, optional
        Mapping ``node -> flow`` (the stationary visit frequency). When given,
        each marker's *area* is set proportional to the node's flow, so its
        *radius* grows as the square root of the flow; the average-flow node
        keeps ``node_size``. Without it every node draws at ``node_size``.

    Returns
    -------
    matplotlib.figure.Figure
        The figure containing the drawing.
    """
    owns_layout = ax is None
    if ax is None:
        fig, ax = plt.subplots(figsize=(5, 5))
    else:
        fig = ax.figure

    ordered = list(graph.nodes())
    color_of = module_colors or module_palette(modules[n] for n in ordered)
    node_colors = [color_of[modules[n]] for n in ordered]

    pos = nx.spring_layout(graph, seed=seed)

    # Node marker area is proportional to flow, so the radius grows as the
    # square root of the flow; the average-flow node keeps ``node_size``.
    # Without flow every node draws at ``node_size``.
    node_sizes = node_size
    if flow is not None:
        flows = [max(flow.get(n, 0.0), 0.0) for n in ordered]
        mean_flow = sum(flows) / len(flows) if flows else 0.0
        if mean_flow > 0:
            node_sizes = [node_size * (f / mean_flow) for f in flows]

    # Edge widths scale with the square root of weight (the link's flow), so a
    # link's width and a node's radius share one convention. Unweighted graphs
    # (all weights equal) draw flat.
    edge_weights = [d.get(weight, 1.0) for _, _, d in graph.edges(data=True)]
    wmax = max(edge_weights, default=1.0)
    widths = [0.5 + 2.5 * math.sqrt(w / wmax) for w in edge_weights]

    # Edges first, nodes on top, so the white node borders stay crisp.
    nx.draw_networkx_edges(
        graph, pos, ax=ax, alpha=0.4, width=widths, edge_color="0.45"
    )
    nx.draw_networkx_nodes(
        graph,
        pos,
        ax=ax,
        nodelist=ordered,
        node_color=node_colors,
        node_size=node_sizes,
        linewidths=0.8,
        edgecolors="white",
    )
    if with_labels:
        nx.draw_networkx_labels(
            graph, pos, ax=ax, font_size=8, font_color="white"
        )
    ax.set_axis_off()
    if owns_layout:
        fig.tight_layout()
    return fig
