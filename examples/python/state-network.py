import infomap

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

myInfomap.addPhysicalNode(1, "PRE")
myInfomap.addPhysicalNode(2, "SCIENCE")
myInfomap.addPhysicalNode(3, "PRL")
myInfomap.addPhysicalNode(4, "BIO")

myInfomap.addStateNode(1, 2)
myInfomap.addStateNode(2, 3)
myInfomap.addStateNode(3, 2)
myInfomap.addStateNode(4, 2)
myInfomap.addStateNode(5, 4)

myInfomap.addLink(1, 2)
myInfomap.addLink(3, 2)
myInfomap.addLink(4, 5)

myInfomap.run()

print(f"Found {myInfomap.numTopModules()} modules with codelength: {myInfomap.codelength()}")

print("\n#node module")
for node,module in myInfomap.getModules().items():
    print(f"{node} {module}")

print("\nState nodes:")
print("#stateId physicalId module")
for node in myInfomap.iterLeafNodes():
    print(f"{node.stateId} {node.physicalId} {node.moduleIndex()}")

print("\nPhysical nodes:")
print("#physicalId module")
for node in myInfomap.iterTreePhysical():
    print(f"{node.physicalId} {node.moduleIndex()}")

