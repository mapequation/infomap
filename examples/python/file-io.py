from infomap import infomap
import pathlib

name = "ninetriangles"
filename = f"../../{name}.net"

# Use the --input flag to read network from file directly
myInfomap = infomap.Infomap(f"--input {filename}")

# You can also read a network with the method below, which by default will accumulate to existing network data
# accumulate = False
# myInfomap.readInputData(filename, accumulate)

myInfomap.run()

maxDepth = myInfomap.maxTreeDepth()

print(f"Found {maxDepth} levels with {myInfomap.numTopModules()} top modules and codelength: {myInfomap.codelength()}")

print("Tree:\n# path flow [physicalId]")
for node in myInfomap.iterTree():
    if node.isLeaf():
        print("{}: {} {}".format(node.path(), node.data.flow, node.physicalId))
    else:
        print("{}: {}".format(node.path(), node.data.flow))

# Iterate leaf modules with iterLeafModules()
# Iterate leaf nodes with iterLeafNodes()

for moduleLevel in range(1, maxDepth):
    print(f"Modules at level {moduleLevel}: {myInfomap.getModules(moduleLevel).values()}")

allModules = myInfomap.getMultilevelModules()
print("\nModules at all levels:")
for nodeId, modules in myInfomap.getMultilevelModules().items():
    print(f"{nodeId}: {modules}")

pathlib.Path("output").mkdir(exist_ok=True)
print(f"Writing bottom level modules to output/{name}.clu...")
myInfomap.writeClu(f"output/{name}.clu")

print(f"Writing top level modules to output/{name}_level1.clu...")
myInfomap.writeClu(f"output/{name}_level1.clu", False, 2)

print(f"Writing tree to output/{name}.tree...")
myInfomap.writeTree(f"output/{name}.tree")

print("Read back .clu file and only calculate codelength...")
infomap2 = infomap.Infomap(f"--input {filename} --no-infomap -c output/{name}.clu -vvv")
infomap2.run()
print(f"Found {infomap2.maxTreeDepth()} levels with {infomap2.numTopModules()} top modules and codelength: {infomap2.codelength()}")

print("Done!")
