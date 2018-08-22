from infomap import infomap

name = "ninetriangles"
filename = "../../{}.net".format(name)

myInfomap = infomap.Infomap("-v -N2 --input {}".format(filename))

myInfomap.run()

print("Found {} top modules with codelength: {}".format(myInfomap.numTopModules(), myInfomap.codelength()))

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


print("Writing top level modules to output/{}_level1.clu...".format(name))
myInfomap.writeClu("output/{}_level1.clu".format(name), False, 1)
# print("Writing second level modules to output/{}_level2.clu...".format(name))
# myInfomap.writeClu("output/{}_level2.clu".format(name), True, 2)

# print("Read back .clu file and check codelength...")
# infomap2 = infomap.Infomap("--input {} --no-infomap -c output/{}_level2.clu -vvv".format(filename, name))
# infomap2.run()
# print("Found {} top modules with codelength: {}".format(infomap2.numTopModules(), infomap2.codelength()))


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
