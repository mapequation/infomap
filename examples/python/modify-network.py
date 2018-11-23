from infomap import infomap


myInfomap = infomap.Infomap("--two-level -v")

# Add weight as an optional third argument
myInfomap.addLink(1, 2)
myInfomap.addLink(1, 3)
myInfomap.addLink(2, 3)
myInfomap.addLink(3, 4)
myInfomap.addLink(4, 5)
myInfomap.addLink(4, 6)
myInfomap.addLink(5, 6)

myInfomap.run()

print(f"Found {myInfomap.numTopModules()} modules with codelength: {myInfomap.codelength()}")

modules = myInfomap.getModules()

print("\n\n\nModify the network and test partition...")

# Do some modification to the network
myInfomap.network().addLink(1, 5)
# Note that removing links will not remove nodes if they become unconnected
myInfomap.network().removeLink(5, 6)

# Run again with the optimal partition from the original network as initial solution
# Set no Infomap to skip optimization and just calculate the codelength
myInfomap.setNoInfomap(True)
myInfomap.run(modules)

print(f"Found {myInfomap.numTopModules()} modules with codelength: {myInfomap.codelength()}")

