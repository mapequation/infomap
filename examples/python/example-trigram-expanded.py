from infomap import infomap

# Notice the --expanded flag to keep the internal higher-order state nodes in the output tree
infomapWrapper = infomap.MemInfomap("--two-level --expanded")

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

print("\n#previousNode node module")
for node in tree.leafIter():
	print("%d %d %d" % (node.stateIndex, node.physIndex, node.moduleIndex()))

"""
Output:
#previousNode node module
2 3 0
3 4 0
2 4 0
3 2 0
4 2 0
2 1 1
0 2 1
1 2 1
2 0 2

Notice that node 2 occurs in two (overlapping) modules.
The expanded output makes it possible to see the conditions
for when node 2 is considered to be in module 0 and 1.
"""