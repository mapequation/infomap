from infomap import infomap

myInfomap = infomap.Infomap()

# from (layer1, node1) to (layer2, node2) with optional weight
myInfomap.addMultilayerLink(2, 1, 1, 2, 1.0)
myInfomap.addMultilayerLink(1, 2, 2, 1, 1.0)
myInfomap.addMultilayerLink(3, 2, 2, 3, 1.0)

myInfomap.run()

print(f"Found {myInfomap.numTopModules()} modules with codelength: {myInfomap.codelength()}")

print("\n#layerId physicalId moduleId:")
for node in myInfomap.iterLeafNodes():
	print(f"{node.layerId} {node.physicalId} {node.moduleIndex()}")

