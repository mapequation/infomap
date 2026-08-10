import pathlib

from infomap import Infomap, Network, Options

im = Infomap()

name = "ninetriangles"
filename = f"../networks/{name}.net"

# You can read a network with the method read_file,
# which by default will accumulate to existing network data
im.read_file(filename, accumulate=False)

result = im.run(num_trials=5)

print(
    f"Found {result.max_depth} levels with {result.num_leaf_modules} leaf modules "
    f"in {result.num_top_modules} top modules and codelength: "
    f"{result.codelength:.8f} bits"
)
print(f"All codelengths: {result.codelengths}")

print("Tree:\n# path node_id module_id flow")
for node in result.nodes():
    print(f"{node.path} {node.node_id} {node.module_id} {node.flow:.8f}")

for module_level in range(1, result.max_depth):
    print(
        f"Modules at level {module_level}: "
        f"{tuple(result.modules(module_level).values())}"
    )

print("\nModules at all levels:")
for node_id, modules in result.multilevel_modules().items():
    print(f"{node_id}: {modules}")

pathlib.Path("output").mkdir(exist_ok=True)
print(f"Writing top level modules to output/{name}.clu...")
result.write(f"output/{name}.clu")

print(f"Writing second level modules to output/{name}_level2.clu...")
result.write(f"output/{name}_level2.clu", depth=2)

print(f"Writing bottom level modules to output/{name}_level-1.clu...")
result.write(f"output/{name}_level-1.clu", depth=-1)

print(f"Writing tree to output/{name}.tree...")
result.write(f"output/{name}.tree")

print("Read back .clu file and only calculate codelength...")
net2 = Network.from_file(filename)
result2 = net2.run(
    options=Options(two_level=True, no_infomap=True, cluster_data=f"output/{name}.clu")
)
print(
    f"Found {result2.max_depth} levels with {result2.num_top_modules} top modules "
    f"and codelength: {result2.codelength:.8f} bits"
)

print("Done!")
