import infomap

im = infomap.Infomap("--two-level --verbose")

# Set the start id for bipartite nodes
im.setBipartiteStartId(5)
# Add weight as an optional third argument
im.addLink(5, 0)
im.addLink(5, 1)
im.addLink(5, 2)
im.addLink(6, 2)
im.addLink(6, 3)
im.addLink(6, 4)

im.run()

print(f"Found {im.numTopModules()} modules with codelength: {im.codelength()}")

print("\n#node flow:")
for node in im.iterLeafNodes():
	print(node.id, node.data.flow)

