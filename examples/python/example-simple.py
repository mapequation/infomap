from infomap import infomap

infomapWrapper = infomap.Infomap("--two-level")

# Add weight as an optional third argument
infomapWrapper.addLink(0, 1)
infomapWrapper.addLink(0, 2)
infomapWrapper.addLink(0, 3)
infomapWrapper.addLink(1, 0)
infomapWrapper.addLink(1, 2)
infomapWrapper.addLink(2, 1)
infomapWrapper.addLink(2, 0)
infomapWrapper.addLink(3, 0)
infomapWrapper.addLink(3, 4)
infomapWrapper.addLink(3, 5)
infomapWrapper.addLink(4, 3)
infomapWrapper.addLink(4, 5)
infomapWrapper.addLink(5, 4)
infomapWrapper.addLink(5, 3)

infomapWrapper.run()

tree = infomapWrapper.tree

print("Found %d modules with codelength: %f" % (tree.numTopModules(), tree.codelength()))

print("\n#node module")
for node in tree.leafIter():
	print("%d %d" % (node.physIndex, node.moduleIndex()))

