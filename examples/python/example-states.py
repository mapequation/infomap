from infomap import infomap

myInfomap = infomap.Infomap("--two-level --verbose")

stateNetwork = """
*Vertices 4
1 "PRE"
2 "SCIENCE"
3 "PRL"
4 "BIO"
# *ngrams
# 1 2 3
# 1 2 2 3
# 4 2 4
*States
1 2 "1 2"
2 3 "2 3"
3 2 "1 2 2"
4 2 "4 2"
5 4 "2 4"
*Links
1 2
3 2
4 5
"""

# Add weight as an optional third argument
network = myInfomap.network()

network.addPhysicalNode(1, "PRE")
network.addPhysicalNode(2, "SCIENCE")
network.addPhysicalNode(3, "PRL")
network.addPhysicalNode(4, "BIO")

network.addStateNode(1, 2)
network.addStateNode(2, 3)
network.addStateNode(3, 2)
network.addStateNode(4, 2)
network.addStateNode(5, 4)

network.addLink(1, 2)
network.addLink(3, 2)
network.addLink(4, 5)

myInfomap.run()

print("Found {} modules with codelength: {}".format(myInfomap.numTopModules(), myInfomap.codelength()))

print("\nState nodes:")
print("#stateId physicalId module")
for node in myInfomap.iterTree():
	if node.isLeaf():
		print("{} {} {}".format(node.stateId, node.physicalId, node.moduleIndex()))

print("\nPhysical nodes:")
print("#physicalId module")
for node in myInfomap.iterTreePhysical():
    if node.isLeaf():
        print("{} {}".format(node.physicalId, node.moduleIndex()))

