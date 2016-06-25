#!/usr/bin/env python

import networkx as nx
import matplotlib.pyplot as plt
import matplotlib.colors as colors

from infomap import infomap

"""
Generate and draw a network with NetworkX, colored
according to the community structure found by Infomap.
"""

def findCommunities(G):
	"""
	Partition network with the Infomap algorithm.
	Annotates nodes with 'community' id and return number of communities found.
	"""
	
	infomapWrapper = infomap.Infomap("--two-level")

	print("Building Infomap network from a NetworkX graph...")
	for e in G.edges_iter():
		infomapWrapper.addLink(*e)

	print("Find communities with Infomap...")
	infomapWrapper.run();

	tree = infomapWrapper.tree

	print("Found %d top modules with codelength: %f" % (tree.numTopModules(), tree.codelength()))

	communities = {}
	for node in tree.leafIter():
		communities[node.originalLeafIndex] = node.moduleIndex()

	nx.set_node_attributes(G, 'community', communities)
	return tree.numTopModules()


def drawNetwork(G):
	# position map
	pos = nx.spring_layout(G)
	# community ids
	communities = [v for k,v in nx.get_node_attributes(G, 'community').items()]
	numCommunities = max(communities) + 1
	# color map from http://colorbrewer2.org/
	cmapLight = colors.ListedColormap(['#a6cee3', '#b2df8a', '#fb9a99', '#fdbf6f', '#cab2d6'], 'indexed', numCommunities)
	cmapDark = colors.ListedColormap(['#1f78b4', '#33a02c', '#e31a1c', '#ff7f00', '#6a3d9a'], 'indexed', numCommunities)

	# edges
	nx.draw_networkx_edges(G, pos)

	# nodes
	nodeCollection = nx.draw_networkx_nodes(G,
		pos = pos,
		node_color = communities,
		cmap = cmapLight
	)
	# set node border color to the darker shade
	darkColors = [cmapDark(v) for v in communities]
	nodeCollection.set_edgecolor(darkColors)

	# Print node labels separately instead
	for n in G.nodes_iter():
		plt.annotate(n,
			xy = pos[n],
			textcoords = 'offset points',
			horizontalalignment = 'center',
			verticalalignment = 'center',
			xytext = [0, 2],
			color = cmapDark(communities[n])
		)

	plt.axis('off')
	# plt.savefig("karate.png")
	plt.show()


G=nx.karate_club_graph()

findCommunities(G)

drawNetwork(G)
