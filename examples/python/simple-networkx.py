import networkx as nx
from infomap import run

G = nx.Graph()
G.add_node(11, node_id=1, layer_id=1)
G.add_node(21, node_id=2, layer_id=1)
G.add_node(22, node_id=2, layer_id=2)
G.add_node(32, node_id=3, layer_id=2)
G.add_edge(11, 21, weight=2)
G.add_edge(22, 32)

# One call from a NetworkX graph to an immutable Result.
result = run(G, silent=True)

for node in sorted(result.nodes(states=True), key=lambda n: n.state_id):
    print(
        node.state_id, node.module_id, f"{node.flow:.2f}", node.node_id, node.layer_id
    )
