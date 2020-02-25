import infomap

eta = 0.3
im = infomap.Infomap(f"--two-level --meta-data-rate {eta}")

# Add weight as an optional third argument
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

im.set_meta_data(0, 1)
im.set_meta_data(1, 1)
im.set_meta_data(2, 2)
im.set_meta_data(3, 2)
im.set_meta_data(4, 3)
im.set_meta_data(5, 3)

im.run()

print(f"\nFound {im.num_top_modules} modules with codelength: {im.codelength}")
Lindex = im.index_codelength
Lmodules = im.module_codelength
Lmeta = im.meta_codelength
Hmeta = im.meta_entropy
print(f" - Codelength = index codelength ({Lindex}) + module codelength ({Lmodules})")
print(f" - Module codelength = {Lmodules - Lmeta} + meta codelength ({Lmeta})")
print(f" - Meta codelength = eta ({eta}) * meta entropy ({Hmeta})")

print("\n#node module meta")
for node in im.nodes:
    print(f"{node.node_id} {node.module_id} {node.meta_data}")
