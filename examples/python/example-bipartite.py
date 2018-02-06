from infomap import infomap

infomapWrapper = infomap.Infomap("--two-level")

# Set the start index for bipartite nodes
infomapWrapper.setBipartiteNodesFrom(5)
# Add weight as an optional third argument
infomapWrapper.addLink(5, 0)
infomapWrapper.addLink(5, 1)
infomapWrapper.addLink(5, 2)
infomapWrapper.addLink(6, 2)
infomapWrapper.addLink(6, 3)
infomapWrapper.addLink(6, 4)

infomapWrapper.run()

tree = infomapWrapper.tree

print("Found %d modules with codelength: %f" % (tree.numTopModules(), tree.codelength()))

print("\n#node module")
for node in tree.leafIter():
	print("%d %d" % (node.physIndex, node.moduleIndex()))

