import infomap


myInfomap = infomap.Infomap("")

markovOrder = 2
myInfomap.addPath([1, 2, 3], markovOrder, 1.0)
myInfomap.addPath([1, 2, 3, 4, 5], markovOrder, 1.0)
myInfomap.addPath([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], markovOrder, 2.0)
myInfomap.addPath([4, 3, 2, 1], markovOrder, 3.0)
myInfomap.addPath([1, 2, 3], markovOrder, 1.0)
myInfomap.addPath([3, 2, 1], markovOrder, 1.0)

print(f"Generated state network with {myInfomap.network().numNodes()} nodes and {myInfomap.network().numLinks()} links")

print("Run Infomap on network...")
myInfomap.run()

print(f"Found {myInfomap.numTopModules()} modules with codelength: {myInfomap.codelength()}")

print("Done!")
