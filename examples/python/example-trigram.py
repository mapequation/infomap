from infomap import infomap

infomapWrapper = infomap.MemInfomap("--two-level")

# Trigrams represents a path from node A through B to C.
# Add link weight as an optional fourth argument
infomapWrapper.addTrigram(0, 2, 0)
infomapWrapper.addTrigram(0, 2, 1)
infomapWrapper.addTrigram(1, 2, 1)
infomapWrapper.addTrigram(1, 2, 0)
infomapWrapper.addTrigram(1, 2, 3)
infomapWrapper.addTrigram(3, 2, 3)
infomapWrapper.addTrigram(2, 3, 4)
infomapWrapper.addTrigram(3, 2, 4)
infomapWrapper.addTrigram(4, 2, 4)
infomapWrapper.addTrigram(4, 2, 3)
infomapWrapper.addTrigram(4, 3, 3)

infomapWrapper.run()

tree = infomapWrapper.tree

print("Found %d modules with codelength: %f" % (tree.numTopModules(), tree.codelength()))

print("\n#node module")
for node in tree.leafIter():
	print("%d %d" % (node.physIndex, node.moduleIndex()))

"""
Output:
#node module
4 0
3 0
2 0
1 1
2 1
0 2

Notice that node 2 occurs in two (overlapping) modules.
"""
