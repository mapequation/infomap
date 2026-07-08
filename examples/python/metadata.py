from infomap import Network, Options

# Changing eta to 2 results in three modules that maps to metadata categories
eta = 1
net = Network()

# Two triangles connected by {2, 3}
net.add_link(0, 1)
net.add_link(0, 2)
net.add_link(2, 1)
net.add_link(2, 3)
net.add_link(3, 4)
net.add_link(3, 5)
net.add_link(4, 5)

net.set_meta_data({0: 2, 1: 2, 2: 1, 3: 1, 4: 3, 5: 3})

result = net.run(options=Options(two_level=True, meta_data_rate=eta))

print(
    f"\nFound {result.num_top_modules} modules with codelength "
    f"{result.codelength:.8f} bits"
)
print(
    f" - Codelength = index codelength ({result.index_codelength:.8f}) "
    f"+ module codelength ({result.module_codelength:.8f})"
)
print(
    f" - Module codelength = "
    f"{(result.module_codelength - result.meta_codelength):.8f} "
    f"+ meta codelength ({result.meta_codelength:.8f})"
)
print(f" - Meta codelength = eta ({eta}) * meta entropy ({result.meta_entropy:.8f})")

print("\n#node module meta")
# meta_data lives on the C++ tree node, so iterate result.tree() leaf nodes.
for node in result.tree():
    if node.is_leaf:
        print(f"{node.node_id} {node.module_id} {node.meta_data}")
