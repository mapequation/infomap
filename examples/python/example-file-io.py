from infomap import infomap

name = "ninetriangles"
filename = "../../{}.net".format(name)

myInfomap = infomap.Infomap("-v -N2 --input {}".format(filename))

myInfomap.run()

print("Found %d top modules with codelength: %f" % (myInfomap.numTopModules(), myInfomap.codelength()))

print("Tree:\n# path flow [physicalId]")
for node in myInfomap.iterTree():
    if node.isLeaf():
        print("{}: {} {}".format(node.path(), node.data.flow, node.physicalId))
    else:
        print("{}: {}".format(node.path(), node.data.flow))

print("Leaf modules:\n# path flow")
for module in myInfomap.iterLeafModules():
    print("{}: {}".format(module.path(), module.data.flow))

print("Leaf nodes:\n# path flow physicalId")
for node in myInfomap.iterLeafNodes():
    print("{}: {} {}".format(node.path(), node.data.flow, node.physicalId))


# print("Writing top level modules to {}_level1.clu...".format(name))
# myInfomap.writeClu("{}_level1.clu".format(name), 1)
# print("Writing second level modules to {}_level2.clu...".format(name))
# myInfomap.writeClu("{}_level2.clu".format(name), 2)

# print("Writing tree to {}.tree...".format(name))
# myInfomap.writeHumanReadableTree("{}.tree".format(name))

# print("\nModules at depth 1:\n#node module")
# for node in myInfomap.leafIter(1):
# 	print("%d %d" % (node.physIndex, node.moduleIndex()))

# print("\nModules at depth 2:\n#node module")
# for node in myInfomap.leafIter(2):
# 	print("%d %d" % (node.physIndex, node.moduleIndex()))

# print("\nModules at lowest level:\n#node module")
# for node in myInfomap.leafIter(-1): # default -1
# 	print("%d %d" % (node.physIndex, node.moduleIndex()))


print("Done!")
