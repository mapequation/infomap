import networkx as nx
import numpy as np
from infomap import Infomap
from sklearn import model_selection

G = nx.karate_club_graph()

grid = model_selection.ParameterGrid(
    {"two_level": [True], "markov_time": np.linspace(0.8, 2, 5), "num_trials": [10], "silent": [True]})

for params in grid:
    im = Infomap(**params)
    im.add_networkx_graph(G)
    im.run()
    print(f"markov time: {params['markov_time']:0.2f} number of leaf modules: {im.num_top_modules}")
