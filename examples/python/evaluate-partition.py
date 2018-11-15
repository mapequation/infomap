from infomap import infomap

# Compare codelengths for two different partitions of a network
# composed of two triangles {0,1,2} and {5,6,7} connected by a 
# chain of two nodes in the middle {3,4}.

# Notice the '--no-infomap' flag, to not try to optimize the partition further
myInfomap = infomap.Infomap("--two-level --verbose --no-infomap --silent")

# Add weight as an optional third argument
myInfomap.addLink(0, 1)
myInfomap.addLink(0, 2)
myInfomap.addLink(1, 2)
myInfomap.addLink(2, 3)
myInfomap.addLink(3, 4)
myInfomap.addLink(4, 5)
myInfomap.addLink(5, 6)
myInfomap.addLink(5, 7)
myInfomap.addLink(6, 7)

# Three modules, with the chain in it's own module
partition1 = {
	0: 0,
	1: 0,
	2: 0,
	3: 1,
	4: 1,
	5: 2,
	6: 2,
	7: 2,
}

# Only two modules, splitting the chain in the middle
partition2 = {
	0: 0,
	1: 0,
	2: 0,
	3: 0,
	4: 2,
	5: 2,
	6: 2,
	7: 2,
}

myInfomap.run(partition1)

print(f"Partition one with {myInfomap.numTopModules()} modules -> codelength: {myInfomap.codelength()}")

myInfomap.run(partition2)

print(f"Partition two with {myInfomap.numTopModules()} modules -> codelength: {myInfomap.codelength()}")

# Output:
# Partition one with 3 modules -> codelength: 2.5555555555555554
# Partition two with 2 modules -> codelength: 2.60715482741224
