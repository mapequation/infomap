import networkx as nx
import numpy as np
from sklearn.model_selection import ParameterGrid

from infomap import Network

# Build the network once, then re-run it with different options.
net = Network.from_networkx(nx.karate_club_graph())

grid = ParameterGrid({"markov_time": np.linspace(0.8, 2, 5)})

for params in grid:
    result = net.run(
        options={"two_level": True, "num_trials": 10, **params}
    )
    print(
        f"markov_time={params['markov_time']:0.1f}: "
        f"number of modules: {result.num_top_modules}"
    )
