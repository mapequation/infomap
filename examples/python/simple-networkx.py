import networkx as nx
from infomap import Infomap

G = nx.Graph()
G.add_node(11, phys_id=1, layer_id=1)
G.add_node(21, phys_id=2, layer_id=1)
G.add_node(22, phys_id=2, layer_id=2)
G.add_node(32, phys_id=3, layer_id=2)
G.add_edge(11, 21)
G.add_edge(22, 32)
im = Infomap(silent=True)
mapping = im.add_networkx_graph(G)
im.run()

for node in sorted(im.nodes, key=lambda n: n.state_id):
    print(node.state_id, node.module_id, f"{node.flow:.2f}", node.node_id)
