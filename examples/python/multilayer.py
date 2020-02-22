import infomap

im = infomap.Infomap("--silent")

# from (layer1, node1) to (layer2, node2) with optional weight
im.add_multilayer_link((2, 1), (1, 2), 1.0)
im.add_multilayer_link((1, 2), (2, 1), 1.0)
im.add_multilayer_link((3, 2), (2, 3), 1.0)

im.run()

print(f"Found {im.num_top_modules} modules with codelength: {im.codelength}")

print("\n#layer_id node_id module_id:")
for node in im.leaf_nodes:
	print(f"{node.layer_id} {node.node_id} {node.module_id}")

