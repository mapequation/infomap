import infomap

print(f"Using Infomap {infomap.__version__}")

im = infomap.Infomap(two_level=True, silent=True)

print("Creating network...")

# Optionally add nodes with names
im.add_node(0, "Node 0")
im.add_node(1, "Node 1")

# Adding links automatically create nodes if not exist.
# Optionally add weight as third argument
im.add_link(0, 1)
im.add_link(0, 2)
im.add_link(0, 3)
im.add_link(1, 0)
im.add_link(1, 2)
im.add_link(2, 1)
im.add_link(2, 0)
im.add_link(3, 0)
im.add_link(3, 4)
im.add_link(3, 5)
im.add_link(4, 3)
im.add_link(4, 5)
im.add_link(5, 4)
im.add_link(5, 3)

print("Run Infomap...")

im.run()

print(f"Found {im.num_top_modules} modules with codelength: {im.codelength}")

print("\n#node_id module_id")
for node, module in im.modules:
    print(f"{node} {module}")

print("\n#node_id module_id path depth child_index flow [name]:")
for node in im.nodes:
    print(
        node.node_id,
        node.module_id,
        node.path,
        node.depth,
        node.child_index,
        node.flow,
        im.get_name(node.node_id, default=node.node_id),
    )

print("\n#path flow enter_flow exit_flow is_leaf")
for node in im.tree:
    print(node.path, node.flow, node.data.enter_flow, node.data.exit_flow, node.is_leaf)


print("\nDone!")
