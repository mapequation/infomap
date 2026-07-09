import json
from matplotlib import cm
from matplotlib import colors as mpl_colors
from pathlib import Path

from infomap.io.networkx import _run_networkx
import matplotlib.pyplot as plt
import networkx as nx
import numpy as np
import pandas as pd
import seaborn as sns

sns.set_palette(sns.color_palette("colorblind"))

try:
    cmap = mpl_colors.ListedColormap(sns.color_palette("colorblind", as_cmap=True))
    cm.register_cmap("colorblind", cmap)
except (AttributeError, ValueError):
    pass


def get_map_equation_example_network():
    """
    Reads the standard map equation example network and returns a networkx
    graph and the nodes' positions.

    Returns
    -------

    """
    with Path("data/map-equation-network.json").open() as fp:
        data = json.load(fp)

    # the network is undirected
    G = nx.Graph()
    pos = dict()

    # read the nodes and their positions
    # insert the nodes into G
    for u in data["nodes"]:
        name = int(u["id"])
        G.add_node(name)
        pos[name] = (float(u["x"]), -float(u["y"]))

    # read the links and insert them into G
    for e in data["links"]:
        u = int(e["source"])
        v = int(e["target"])
        w = int(e["weight"])
        G.add_edge(u, v, weight=w)

    nx.set_node_attributes(G, pos, "pos")

    return G, pos


def plot_network(G, node_color_attr="module", **kwargs):
    _, ax = plt.subplots(1, 1, figsize=(4, 4))

    if "pos" in kwargs:
        pos = kwargs["pos"]
        del kwargs["pos"]
    else:
        pos = nx.get_node_attributes(G, "pos")
        if len(pos) == 0:
            pos = nx.kamada_kawai_layout(G)

    def brighten(color):
        c = np.array(list(color)[:3])
        return tuple(float(x) for x in 0.5 * c + 0.5)

    node_color_attributes = nx.get_node_attributes(G, node_color_attr)
    if len(node_color_attributes) == 0:
        node_color_map = {n: (0.8, 0.8, 0.8) for n in G.nodes}
        n_modules = 0
    else:
        modules = set(node_color_attributes.values())
        module_to_index = {m: i for i, m in enumerate(modules)}
        n_modules = len(modules)
        palette = sns.color_palette(n_colors=n_modules)
        node_color_map = {
            n: palette[module_to_index[node_color_attributes[n]]]
            for n in node_color_attributes
        }

    node_colors = [node_color_map.get(n, (0.8, 0.8, 0.8)) for n in G.nodes]

    # edge color: blend endpoint colors, then brighten halfway to white
    edge_colors = []
    for u, v in G.edges:
        cu = np.array(list(node_color_map.get(u, (0.8, 0.8, 0.8)))[:3])
        cv = np.array(list(node_color_map.get(v, (0.8, 0.8, 0.8)))[:3])
        edge_colors.append(brighten((cu + cv) / 2))

    title = (
        "" if n_modules == 0 else f"{G.graph['M']} modules, {G.graph['L']:.2f} bits."
    )

    nx.draw_networkx(
        G=G,
        pos=pos,
        ax=ax,
        node_color=node_colors,
        width=[w for _, _, w in G.edges.data("weight")],
        edge_color=edge_colors,
        arrows=True,
        connectionstyle="arc3,rad=0.1",
        edgecolors="white",
        font_size=10,
        with_labels=False,
        **kwargs,
    )

    # draw labels per color group with that color brightened halfway to white
    color_to_nodes = {}
    for n in G.nodes:
        key = tuple(round(x, 6) for x in node_color_map.get(n, (0.8, 0.8, 0.8))[:3])
        color_to_nodes.setdefault(key, []).append(n)

    for color_key, nodes in color_to_nodes.items():
        nx.draw_networkx_labels(
            G=G,
            pos=pos,
            ax=ax,
            labels={n: n for n in nodes},
            font_color=brighten(color_key),
            font_size=10,
        )

    ax.axis("off")
    ax.set(title=title)
    plt.show()


def _module_series(infomap_result, node_mapping):
    modules = {
        node_mapping[node.state_id]: node.module_id
        for node in infomap_result.nodes
        if node.state_id in node_mapping
    }
    return pd.Series(modules, name="module_id")


def partition(G, initial_partition=None, **infomap_args):
    im, node_mapping = _run_networkx(
        G,
        initial_partition=initial_partition,
        **infomap_args,
    )
    modules = _module_series(im, node_mapping)
    nx.set_node_attributes(G, modules.to_dict(), "module")
    G.graph["M"] = im.num_top_modules
    G.graph["L"] = im.codelength
    G.graph["L_ind"] = im.index_codelength
    G.graph["L_mod"] = im.module_codelength
    return modules


def plot_modular_network(
    G,
    pos=None,
    node_color_attr="module",
    figsize=(4, 4),
    node_size=300,
    edge_width=None,
    brighten_factor=0.5,
    use_value_as_palette_index=False,
    shape_attr=None,
    shape_map=None,
    show_labels=False,
    label_attr=None,
    bipartite_anchor_attr=None,
    bipartite_anchor_value=None,
    arrows=True,
    connectionstyle="arc3,rad=0.1",
    node_edgecolors=None,
    font_size=10,
):
    _, ax = plt.subplots(1, 1, figsize=figsize)

    offset = 1.0 - brighten_factor

    def brighten(color):
        c = np.array(list(color)[:3])
        return tuple(float(x) for x in brighten_factor * c + offset)

    # Resolve node positions
    if pos is None:
        if bipartite_anchor_attr is not None:
            anchors = [
                n
                for n in G
                if G.nodes[n].get(bipartite_anchor_attr) == bipartite_anchor_value
            ]
            pos = nx.bipartite_layout(G, anchors)
        else:
            pos = nx.get_node_attributes(G, "pos")
            if len(pos) == 0:
                pos = nx.kamada_kawai_layout(G)

    # Resolve color maps. ``edge_color_map`` is always derived from the
    # module-to-index mapping (used for edge-color blending and as a fallback
    # for node display). ``display_colors`` is what nodes/labels actually use;
    # when ``use_value_as_palette_index`` is True, it instead indexes the
    # palette by ``attribute_value - 1`` (matching the original bipartite and
    # metadata helpers).
    node_color_attributes = nx.get_node_attributes(G, node_color_attr)
    if len(node_color_attributes) == 0:
        edge_color_map = {n: (0.8, 0.8, 0.8) for n in G.nodes}
        display_colors = dict(edge_color_map)
    else:
        modules = set(node_color_attributes.values())
        module_to_index = {m: i for i, m in enumerate(modules)}
        n_modules = len(modules)
        palette = sns.color_palette(n_colors=n_modules)

        edge_color_map = {
            n: palette[module_to_index[node_color_attributes[n]]]
            for n in node_color_attributes
        }
        for n in G.nodes:
            edge_color_map.setdefault(n, (0.8, 0.8, 0.8))

        if use_value_as_palette_index:
            display_colors = {
                n: palette[node_color_attributes[n] - 1] for n in node_color_attributes
            }
            for n in G.nodes:
                display_colors.setdefault(n, (0.8, 0.8, 0.8))
        else:
            display_colors = dict(edge_color_map)

    # Edge colors: blend endpoint colors, then brighten toward white
    edge_colors = []
    for u, v in G.edges:
        cu = np.array(list(edge_color_map.get(u, (0.8, 0.8, 0.8)))[:3])
        cv = np.array(list(edge_color_map.get(v, (0.8, 0.8, 0.8)))[:3])
        edge_colors.append(brighten((cu + cv) / 2))

    # Draw nodes (optionally grouped by a shape attribute)
    if shape_attr is not None and shape_map is not None:
        for value, shape in shape_map.items():
            nodelist = [n for n in G if G.nodes[n].get(shape_attr) == value]
            if not nodelist:
                continue
            node_kw = dict(
                G=G,
                pos=pos,
                ax=ax,
                nodelist=nodelist,
                node_color=[display_colors[n] for n in nodelist],
                node_shape=shape,
                node_size=node_size,
            )
            if node_edgecolors is not None:
                node_kw["edgecolors"] = node_edgecolors
            nx.draw_networkx_nodes(**node_kw)
    else:
        node_kw = dict(
            G=G,
            pos=pos,
            ax=ax,
            node_color=[display_colors[n] for n in G.nodes],
            node_size=node_size,
        )
        if node_edgecolors is not None:
            node_kw["edgecolors"] = node_edgecolors
        nx.draw_networkx_nodes(**node_kw)

    # Draw edges. We forward ``node_size`` only when the caller explicitly
    # asked for a non-default value, so that the bipartite and metadata cases
    # (which previously did not pass ``node_size`` to ``draw_networkx_edges``)
    # keep their original arrow geometry.
    edge_kw = dict(
        G=G,
        pos=pos,
        ax=ax,
        edge_color=edge_colors,
        arrows=arrows,
        connectionstyle=connectionstyle,
    )
    if edge_width is not None:
        edge_kw["width"] = edge_width
    if node_size != 300:
        edge_kw["node_size"] = node_size
    nx.draw_networkx_edges(**edge_kw)

    # Draw labels grouped by node color so each group gets its own brightened color
    if show_labels:
        color_to_nodes = {}
        for n in G.nodes:
            key = tuple(round(x, 6) for x in display_colors[n][:3])
            color_to_nodes.setdefault(key, []).append(n)
        for color_key, nodes in color_to_nodes.items():
            if label_attr:
                labels = {n: G.nodes[n][label_attr] for n in nodes}
            else:
                labels = {n: n for n in nodes}
            nx.draw_networkx_labels(
                G=G,
                pos=pos,
                ax=ax,
                labels=labels,
                font_color=brighten(color_key),
                font_size=font_size,
            )

    ax.axis("off")
    plt.show()
