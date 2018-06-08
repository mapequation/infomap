from infomap import infomap

name = "ninetriangles"
filename = "../../{}.net".format(name)

myInfomap = infomap.Infomap("-v --input {}".format(filename))

myInfomap.run()

print("Found %d top modules with codelength: %f" % (myInfomap.numTopModules(), myInfomap.codelength()))

print("Tree:")
for node in myInfomap.iterTree():
    if node.isLeaf():
        print("{}: {} {}".format(node.path(), node.data.flow, node.physicalId))
    elif node.isLeafModule():
        print("{}: {} *".format(node.path(), node.data.flow))
    else:
        print("{}: {}".format(node.path(), node.data.flow))

print("Leaf modules:")
for module in myInfomap.iterLeafModules():
    print("{}: {}".format(module.path(), module.data.flow))

print("Leaf nodes:")
for node in myInfomap.iterLeafNodes():
    print("{}: {} {}".format(node.path(), node.data.flow, node.physicalId))


# print("Writing top level modules to %s_level1.clu..." % name)
# tree.writeClu("%s_level1.clu" % name, 1)
# print("Writing second level modules to %s_level2.clu..." % name)
# tree.writeClu("%s_level2.clu" % name, 2)

# print("Writing tree to %s.tree..." % name)
# tree.writeHumanReadableTree("%s.tree" % name)

# print("\nModules at depth 1:\n#node module")
# for node in tree.leafIter(1):
# 	print("%d %d" % (node.physIndex, node.moduleIndex()))

# print("\nModules at depth 2:\n#node module")
# for node in tree.leafIter(2):
# 	print("%d %d" % (node.physIndex, node.moduleIndex()))

# print("\nModules at lowest level:\n#node module")
# for node in tree.leafIter(-1): # default -1
# 	print("%d %d" % (node.physIndex, node.moduleIndex()))


print("Done!")
