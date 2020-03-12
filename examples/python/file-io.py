import infomap
import pathlib

name = "ninetriangles"
filename = f"../../{name}.net"

im = infomap.Infomap()

# You can read a network with the method read_file,
# which by default will accumulate to existing network data
accumulate = False
im.read_file(filename, accumulate)

im.run("-N5")

print(f"Found {im.max_depth} levels with {im.num_leaf_modules} leaf modules in {im.num_top_modules} top modules and codelength: {im.codelength}")
print(f"All codelengths: {im.codelengths}")

print("Tree:\n# path node_id module_id flow")
for node in im.nodes:
    print(f"{node.path} {node.node_id} {node.module_id} {node.flow}")

for module_level in range(1, im.max_depth):
    print(f"Modules at level {module_level}: {im.get_modules(module_level).values()}")

print("\nModules at all levels:")
for node_id, modules in im.get_multilevel_modules().items():
    print(f"{node_id}: {modules}")

pathlib.Path("output").mkdir(exist_ok=True)
print(f"Writing top level modules to output/{name}.clu...")
im.write_clu(f"output/{name}.clu")

print(f"Writing second level modules to output/{name}_level2.clu...")
im.write_clu(f"output/{name}_level2.clu", depth_level=2)

print(f"Writing bottom level modules to output/{name}_level-1.clu...")
im.write_clu(f"output/{name}_level-1.clu", depth_level=-1)

print(f"Writing tree to output/{name}.tree...")
im.write_tree(f"output/{name}.tree")

print("Read back .clu file and only calculate codelength...")
im2 = infomap.Infomap(f"--input {filename} --no-infomap -c output/{name}.clu")
im2.run()
print(f"Found {im2.max_depth} levels with {im2.num_top_modules} top modules and codelength: {im2.codelength}")

print("Done!")
