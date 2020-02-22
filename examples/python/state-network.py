import infomap

im = infomap.Infomap("--two-level --verbose")

stateNetwork = """
*Vertices 4
1 "PRE"
2 "SCIENCE"
3 "PRL"
4 "BIO"
# *ngrams
# 1 2 3
# 1 2 2 3
# 4 2 4
*States
1 2 "1 2"
2 3 "2 3"
3 2 "1 2 2"
4 2 "4 2"
5 4 "2 4"
*Links
1 2
3 2
4 5
"""

im.set_name(1, "PRE")
im.set_name(2, "SCIENCE")
im.set_name(3, "PRL")
im.set_name(4, "BIO")

im.add_state_node(1, 2)
im.add_state_node(2, 3)
im.add_state_node(3, 2)
im.add_state_node(4, 2)
im.add_state_node(5, 4)

im.add_link(1, 2)
im.add_link(3, 2)
im.add_link(4, 5)

im.run()

print(f"Found {im.num_top_modules} modules with codelength: {im.codelength}")

print("\n#node_id module")
for node, module in im.modules:
    print(f"{node} {module}")

print("\nState nodes:")
print("#state_id node_id module_id")
for node in im.leaf_nodes:
    print(f"{node.state_id} {node.node_id} {node.module_id}")

print("\nPhysical nodes (merging state nodes with same physical node id within modules):")
print("#node_id module_id")
for node in im.physical_tree:
    if node.is_leaf:
        print(f"{node.node_id} {node.module_id}")

# for node in im.physical_leaf_nodes:
#     print(f"{node.node_id} {node.module_id}")

