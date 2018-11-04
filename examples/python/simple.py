from infomap import infomap

myInfomap = infomap.Infomap("--two-level --verbose")

# Add weight as an optional third argument
myInfomap.addLink(0, 1)
myInfomap.addLink(0, 2)
myInfomap.addLink(0, 3)
myInfomap.addLink(1, 0)
myInfomap.addLink(1, 2)
myInfomap.addLink(2, 1)
myInfomap.addLink(2, 0)
myInfomap.addLink(3, 0)
myInfomap.addLink(3, 4)
myInfomap.addLink(3, 5)
myInfomap.addLink(4, 3)
myInfomap.addLink(4, 5)
myInfomap.addLink(5, 4)
myInfomap.addLink(5, 3)

myInfomap.run()

print(f"Found {myInfomap.numTopModules()} modules with codelength: {myInfomap.codelength()}")

print("\n#node module")
for node,module in myInfomap.getModules().items():
	print(f"{node} {module}")
	
