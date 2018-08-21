from infomap import infomap


myInfomap = infomap.Infomap("")

network = myInfomap.network()

markovOrder = 2
network.addPath([1, 2, 3], markovOrder, 1.0)
network.addPath([1, 2, 3, 4, 5], markovOrder, 1.0)
network.addPath([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], markovOrder, 2.0)
network.addPath([4, 3, 2, 1], markovOrder, 3.0)
network.addPath([1, 2, 3], markovOrder, 1.0)
network.addPath([3, 2, 1], markovOrder, 1.0)

print("Generated state network with {} nodes and {} links".format(network.numNodes(), network.numLinks()))

print("Run Infomap on network...")
myInfomap.run()

print("Found {} top modules with codelength: {}".format(myInfomap.numTopModules(), myInfomap.codelength()))

print("Done!")
