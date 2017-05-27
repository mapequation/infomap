from infomap import infomap as im

name = "ninetriangles"
filename = "../../{}.net".format(name)


myInfomap = im.Infomap("--input {}".format(filename))

print(dir(myInfomap))
myInfomap.asdf()

# print("Found %d top modules with codelength: %f" % (myInfomap.numTopModules(), myInfomap.getCodelength()))
# print("Found %d top modules with codelength: %f" % (tree.numTopModules(), tree.codelength()))

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
