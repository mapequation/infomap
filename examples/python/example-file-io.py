from infomap import infomap

name = "ninetriangles"
filename = "../../{}.net".format(name)


infomapWrapper = infomap.Infomap("-N5 --silent")

infomapWrapper.readInputData(filename)

infomapWrapper.run()

tree = infomapWrapper.tree

print("Found %d top modules with codelength: %f" % (tree.numTopModules(), tree.codelength()))

print("Writing top level modules to %s_level1.clu..." % name)
tree.writeClu("%s_level1.clu" % name, 1)
print("Writing second level modules to %s_level2.clu..." % name)
tree.writeClu("%s_level2.clu" % name, 2)

print("Writing tree to %s.tree..." % name)
tree.writeHumanReadableTree("%s.tree" % name)

print("Done!")
