from infomap import infomap

myInfomap = infomap.Infomap("--two-level --verbose")

# Add weight as an optional third argument
network = infomap.Network()
network.addLink(0, 1)
network.addLink(0, 2)
network.addLink(0, 3)
network.addLink(1, 0)
network.addLink(1, 2)
network.addLink(2, 1)
network.addLink(2, 0)
network.addLink(3, 0)
network.addLink(3, 4)
network.addLink(3, 5)
network.addLink(4, 3)
network.addLink(4, 5)
network.addLink(5, 4)
network.addLink(5, 3)

myInfomap.run(network)

print("Found %d modules with codelength: %f" % (myInfomap.numTopModules(), myInfomap.codelength()))

print("\n#node module")

print("Tree:")
for it in myInfomap.tree():
	node = it.current()
	if node.isLeaf():
		print("{} {}".format(node.physicalId, it.moduleIndex()))
