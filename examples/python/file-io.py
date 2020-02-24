import infomap
import pathlib

name = "ninetriangles"
filename = f"../../{name}.net"

# Use the --input flag to read network from file directly
im = infomap.Infomap(f"--input {filename}")

# You can also read a network with the method below,
# which by default will accumulate to existing network data
# accumulate = False
# im.read_file(filename, accumulate)

im.run()

print(f"Found {im.max_depth} levels with {im.num_top_modules} top modules and codelength: {im.codelength}")

print("Tree:\n# path node_id flow")
for node in im.leaf_nodes:
    print(f"{node.path} {node.node_id} {node.flow}")

for module_level in range(1, im.max_depth):
    print(f"Modules at level {module_level}: {im.get_modules(module_level).values()}")

print("\nModules at all levels:")
for node_id, modules in im.get_multilevel_modules().items():
    print(f"{node_id}: {modules}")

pathlib.Path("output").mkdir(exist_ok=True)
print(f"Writing bottom level modules to output/{name}.clu...")
im.write_clu(f"output/{name}.clu")

print(f"Writing top level modules to output/{name}_level1.clu...")
im.write_clu(f"output/{name}_level1.clu", depth_level=2)

print(f"Writing tree to output/{name}.tree...")
im.write_tree(f"output/{name}.tree")

print("Read back .clu file and only calculate codelength...")
im2 = infomap.Infomap(f"--input {filename} --no-infomap -c output/{name}.clu -vvv")
im2.run()
print(f"Found {im2.max_depth} levels with {im2.num_top_modules} top modules and codelength: {im2.codelength}")

print("Done!")
