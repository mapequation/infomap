#!/usr/bin/env python
import os.path
from infomap import infomap

conf = infomap.init("--silent -N5")
# Add output directory (and output name) to automatically write result to file
# conf = infomap.init("--silent -N5 . --out-name test")

filename = "../../ninetriangles.net"
name = os.path.splitext(os.path.basename(filename))[0]
print("Loading network from '%s'..." % filename)
network = infomap.Network(conf)
network.readInputData(filename)

print("Running Infomap...")
tree = infomap.HierarchicalNetwork(conf)
infomap.run(network, tree)

print("Found %d top modules with codelength: %f" % (tree.numTopModules(), tree.codelength()))

print("Writing top level clusters to %s_level1.clu..." % name)
tree.writeClu("%s_level1.clu" % name, 1)
print("Writing second level clusters to %s_level2.clu..." % name)
tree.writeClu("%s_level2.clu" % name, 2)

print("Writing tree to %s.tree..." % name)
tree.writeHumanReadableTree("%s.tree" % name)

print("Done!")
