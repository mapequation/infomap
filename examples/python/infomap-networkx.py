import pathlib

import matplotlib.colors as colors
import matplotlib.pyplot as plt
import networkx as nx

from infomap import Infomap

"""
Generate and draw a network with NetworkX, colored
according to the community structure found by Infomap.
"""


def draw_network(G):
    # position map
    pos = nx.spring_layout(G)
    # community index
    communities = [c - 1 for c in nx.get_node_attributes(G, "community").values()]
    num_communities = max(communities) + 1

    # color map from http://colorbrewer2.org/
    cmap_light = colors.ListedColormap(
        ["#a6cee3", "#b2df8a", "#fb9a99", "#fdbf6f", "#cab2d6"],
        "indexed",
        num_communities,
    )
    cmap_dark = colors.ListedColormap(
        ["#1f78b4", "#33a02c", "#e31a1c", "#ff7f00", "#6a3d9a"],
        "indexed",
        num_communities,
    )

    # edges
    nx.draw_networkx_edges(G, pos)

    # nodes
    node_collection = nx.draw_networkx_nodes(
        G, pos=pos, node_color=communities, cmap=cmap_light
    )

    # set node border color to the darker shade
    dark_colors = [cmap_dark(v) for v in communities]
    node_collection.set_edgecolor(dark_colors)

    # Print node labels separately instead
    for n in G.nodes:
        plt.annotate(
            n,
            xy=pos[n],
            textcoords="offset points",
            horizontalalignment="center",
            verticalalignment="center",
            xytext=[0, 2],
            color=cmap_dark(communities[n]),
        )

    plt.axis("off")
    pathlib.Path("output").mkdir(exist_ok=True)
    print("Writing network figure to output/karate.png")
    plt.savefig("output/karate.png")
    # plt.show()


G = nx.karate_club_graph()

print("Building Infomap network from a NetworkX graph...")
im = Infomap(two_level=True, silent=True, num_trials=20)
im.add_networkx_graph(G)

print("Find communities with Infomap...")
im.run()

print(f"Found {im.num_top_modules} modules with codelength {im.codelength:.8f} bits")

nx.set_node_attributes(G, im.get_modules(), "community")

draw_network(G)

print("\nA state network...")
G = nx.Graph()
G.add_node("a", phys_id=1)
G.add_node("b", phys_id=2)
G.add_node("c", phys_id=3)
G.add_node("d", phys_id=1)
G.add_node("e", phys_id=4)
G.add_node("f", phys_id=5)
G.add_edge("a", "b")
G.add_edge("a", "c")
G.add_edge("b", "c")
G.add_edge("d", "e")
G.add_edge("d", "f")
G.add_edge("e", "f")
im = Infomap(silent=True)
im.add_networkx_graph(G)
im.run()
print("#node_id module_id flow state_id")
for node in im.nodes:
    print(node.node_id, node.module_id, node.flow, node.state_id)
# 1 1 0.16666666666666666 0
# 2 1 0.16666666666666666 1
# 3 1 0.16666666666666666 2
# 1 2 0.16666666666666666 3
# 4 2 0.16666666666666666 4
# 5 2 0.16666666666666666 5


print("\nA multilayer network...")
G = nx.Graph()
G.add_node(11, phys_id=1, layer_id=1)
G.add_node(21, phys_id=2, layer_id=1)
G.add_node(22, phys_id=2, layer_id=2)
G.add_node(23, phys_id=3, layer_id=2)
G.add_edge(11, 21)
G.add_edge(22, 23)
im = Infomap(silent=False)
# Add multilayer_inter_intra_format=False for full multilayer format
im.add_networkx_graph(G)
im.run()
print("#node_id module_id flow state_id layer_id")
for node in im.nodes:
    print(
        node.node_id, node.module_id, f"{node.flow:.2f}", node.state_id, node.layer_id
    )
# 1 1 0.25 0 1
# 2 1 0.25 1 1
# 2 2 0.25 2 2
# 3 2 0.25 3 2
