import numpy as np

import sys
community = str(sys.argv[1])

N = 1000

prior = np.log(N)

# -------------- network --------------

f_network = open("network.net", "r") # input network

En1, En2 = [], []
for line in f_network:
    [node1, node2, weight] = line.split(" ")
    En1.append(int(node1) - 1) # node index should start from 0
    En2.append(int(node2) - 1) # node index should start from 0       
f_network.close()

# Associate nodes to modules (planted partition)    
f_community = open(community + ".clu", "r")
node_id, module_id = [], [] 
for line in f_community:
    [n_id, m_id, flow] = line.split(" ")
    node_id.append(int(n_id) - 1) # node index should start from 0
    module_id.append(int(m_id))
f_community.close()
node_id, module_id = np.array(node_id), np.array(module_id)
module_id = module_id[np.argsort(node_id)]
modules = np.unique(module_id) # list of unique module ids
m = len(modules) # number of modules

# -------------- map equation --------------

k = np.zeros(N) # node degree
n_i_enter = np.zeros(m) # number of links to(from) module
for alpha, beta in zip(En1, En2):
    k[alpha] += 1
    k[beta] += 1
    i = module_id[alpha]
    j = module_id[beta]
    if i != j:
        n_i_enter[i] += 1.0        
        n_i_enter[j] += 1.0     
        
k += prior           
for i in range(m):
    module_size = len(module_id[module_id == i])
    n_i_enter[i] += prior * (N - module_size) * module_size / (N - 1.0)            

normM = 1.0 / np.float(np.sum(k))

# H_Q: the average length of codewords in the index codebook
x = n_i_enter[n_i_enter > 0]
if len(x) > 1:   
    H_Q = (np.log(np.sum(x)) - (1.0 / np.sum(x)) * np.sum(x * np.log(x))) / np.log(2)              
else:
    H_Q = 0.0
        
# H_Pi: the average length of codewords in module codebook i
H_Pi = np.zeros(m)
for i in modules:
    n_i = np.insert(k[module_id == i], 0, n_i_enter[i])
    n_i = n_i[n_i > 0]
    if len(n_i) > 0:        
        H_Pi[i] = (np.log(np.sum(n_i)) - (1.0 / np.sum(n_i)) * np.sum(n_i * np.log(n_i))) / np.log(2)
        
# q_i_enter: the module transition rates at which the random walker enters and exits module i  
q_i_enter = n_i_enter * normM

# p_i: the rate at which the module codebook i is used        
p_i = np.zeros(m)    
for i in modules:
    p_i[i] += np.sum(k[module_id == i]) * normM    
p_i = p_i + q_i_enter
        
# L_M: the average codelength; L_i: index codebook; L_m: module codebook
L_i = np.sum(q_i_enter) * H_Q
L_m = np.sum(p_i * H_Pi)
L_M = L_i + L_m    
    
print(str(L_m) +" + " + str(L_i) + " = " + str(L_M))