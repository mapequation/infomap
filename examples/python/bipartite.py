import infomap

im = infomap.Infomap("--two-level --verbose")

# Set the start id for bipartite nodes
im.bipartite_start_id = 5

# Add weight as an optional third argument
im.add_link(5, 0)
im.add_link(5, 1)
im.add_link(5, 2)
im.add_link(6, 2)
im.add_link(6, 3)
im.add_link(6, 4)

im.run()

print(f"Found {im.num_top_modules} modules with codelength: {im.codelength}")

print("\n#node flow:")
for node in im.leaf_nodes:
    print(node.node_id, node.flow)
