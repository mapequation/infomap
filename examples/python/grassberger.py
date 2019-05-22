from infomap import infomap

im = infomap.Infomap("--two-level --grassberger")

# Add weight as an optional third argument
im.addLink(0, 1)
im.addLink(0, 2)
im.addLink(0, 3)
im.addLink(1, 0)
im.addLink(1, 2)
im.addLink(2, 1)
im.addLink(2, 0)
im.addLink(3, 0)
im.addLink(3, 4)
im.addLink(3, 5)
im.addLink(4, 3)
im.addLink(4, 5)
im.addLink(5, 4)
im.addLink(5, 3)

im.run()

print(f"Found {im.numTopModules()} modules with codelength: {im.codelength()}")

print("\n#node flow:")
for node in im.iterLeafNodes():
	print(node.id, node.getFlowInt())

