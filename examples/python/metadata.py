from infomap import Infomap

# Changing eta to 2 results in three modules that maps to metadata categories
eta = 1
im = Infomap(two_level=True, silent=True, meta_data_rate=eta)

# Two triangles connected by {2, 3}
im.add_link(0, 1)
im.add_link(0, 2)
im.add_link(2, 1)
im.add_link(2, 3)
im.add_link(3, 4)
im.add_link(3, 5)
im.add_link(4, 5)

im.set_meta_data(0, 2)
im.set_meta_data(1, 2)
im.set_meta_data(2, 1)
im.set_meta_data(3, 1)
im.set_meta_data(4, 3)
im.set_meta_data(5, 3)

result = im.run()

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
