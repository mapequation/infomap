from infomap import infomap

infomapWrapper = infomap.MemInfomap("--two-level --expanded")

# from (layer, node) to (layer, node) weight
infomapWrapper.addMultiplexLink(2, 1, 1, 2, 1.0)
infomapWrapper.addMultiplexLink(1, 2, 2, 1, 1.0)
infomapWrapper.addMultiplexLink(3, 2, 2, 3, 1.0)

infomapWrapper.run()

tree = infomapWrapper.tree

print("Found %d modules with codelength: %f" % (tree.numTopModules(), tree.codelength()))

print("\n#layer node module:")
for node in tree.leafIter():
	print("%d %d %d" % (node.stateIndex, node.physIndex, node.moduleIndex()))

