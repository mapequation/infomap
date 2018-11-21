from infomap import infomap


myInfomap = infomap.Infomap("--two-level")

# Add weight as an optional third argument
myInfomap.addLink(1, 2)
myInfomap.addLink(1, 3)
myInfomap.addLink(2, 3)
myInfomap.addLink(3, 4)
myInfomap.addLink(4, 5)
myInfomap.addLink(4, 6)
myInfomap.addLink(5, 6)
myInfomap.addLink(1, 5)

myInfomap.run()

print(f"Found {myInfomap.numTopModules()} modules with codelength: {myInfomap.codelength()}")

print("\n\n\nRemoving link...")

# Note that this method will not remove nodes if they become unconnected
myInfomap.network().removeLink(1, 5)

myInfomap.run()

print(f"Found {myInfomap.numTopModules()} modules with codelength: {myInfomap.codelength()}")

