import numpy as np
import sys
import networkx as nx

input_path = sys.argv[1]
output_path = sys.argv[2]
prior_scaling = float(sys.argv[3]) if len(sys.argv) >= 4 else 1
print(f"Generate Bayesian posterior network from input network '{input_path}' with prior scaling '{prior_scaling}'...")

try:
  print("Try reading edge list...")
  G = nx.read_edgelist(input_path, nodetype=int, create_using=nx.DiGraph, data=(("weight", float),))
except Exception as e:
  print(e)
  print("Try reading pajek...")
  G = nx.read_pajek(input_path)


N = G.number_of_nodes()
E = G.number_of_edges()
print(f"Parsed network with {N} nodes and {E} edges...")

# read edgelist and create adjacency matrix
A = np.zeros(N * N).reshape(N, N) # adjacency matrix
W = np.zeros(N * N).reshape(N, N) # weighted network
for source, target, data in G.edges(data=True):
    w = 1 if 'weight' not in data else data['weight']
    node_i, node_j = int(source), int(target)
    A[int(node_i)-1][int(node_j)-1] = 1
    W[int(node_i)-1][int(node_j)-1] += w


#create prior network
s_in, k_in, mass_in, s_out, k_out, mass_out = np.zeros(N), np.zeros(N), np.ones(N), np.zeros(N), np.zeros(N), np.ones(N)
for node in range(N):
    s_out[node] = np.sum(W[node, :])    
    k_out[node] = np.sum(A[node, :])
    if k_out[node] > 0:
        mass_out[node] = np.float(s_out[node]) / k_out[node]
    s_in[node] = np.sum(W[:, node])
    k_in[node] = np.sum(A[:, node])
    if k_in[node] > 0:
        mass_in[node] = np.float(s_in[node]) / k_in[node]
norm = prior_scaling * (np.log(N) / (N - 1)) * (np.sum(k_out) + np.sum(k_in)) / (np.sum(s_out) + np.sum(s_in))        
# TODO: Added scaling factor above as ln(N)/N is too strong for small networks

# print posterior network
count_num_edges = 0
with open(output_path, 'w') as fout:
    fout.write(f"# {' '.join(sys.argv)}\n")
    fout.write(f"*Vertices {N}\n")
    for node in range(1, N + 1):
        fout.write(f"{node} \"{node}\"\n")
    fout.write(f"*Edges {N * (N - 1)}\n")
    for node_i in range(N):
        for node_j in range(N):
            if node_i == node_j:
                continue
            weight = W[node_i][node_j] + norm * mass_out[node_i] * mass_in[node_j]
            # if node_j == 0:
            #     print(f"{node_i} -> {node_j}, norm: {norm}, mass_out_i: {mass_out[node_i]}, mass_in_j: {mass_in[node_j]}")
            # if node_i == 0:
            #     print(f"{node_i} -> {node_j}, norm: {norm}, mass_out_i: {mass_out[node_i]}, mass_in_j: {mass_in[node_j]}")
            fout.write(f"{node_i + 1} {node_j + 1} {weight}\n")
            count_num_edges += 1

print(f"Wrote posterior network with {N} nodes and {count_num_edges} edges to '{output_path}'!")
