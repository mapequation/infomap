from infomap import infomap

eta = 0.3
im = infomap.Infomap(f"--two-level --meta-data-rate {eta}")

# Add weight as an optional third argument
im.addLink(0, 1)
im.addLink(0, 2)
im.addLink(0, 3)
im.addLink(1, 0)
im.addLink(1, 2)
im.addLink(2, 1)
im.addLink(2, 0)
im.addLink(3, 0)
im.addLink(3, 4)
im.addLink(3, 5)
im.addLink(4, 3)
im.addLink(4, 5)
im.addLink(5, 4)
im.addLink(5, 3)

network = im.network()
network.addMetaData(0, 1)
network.addMetaData(1, 1)
network.addMetaData(2, 2)
network.addMetaData(3, 2)
network.addMetaData(4, 3)
network.addMetaData(5, 3)

im.run()

print(f"\nFound {im.numTopModules()} modules with codelength: {im.codelength()}")
Lindex = im.getIndexCodelength()
Lmodules = im.getModuleCodelength()
Lmeta = im.getMetaCodelength()
Hmeta = im.getMetaCodelength(True)
print(f" - Codelength = index codelength ({Lindex}) + module codelength ({Lmodules})")
print(f" - Module codelength = {Lmodules - Lmeta} + meta codelength ({Lmeta})")
print(f" - Meta codelength = eta ({eta}) * meta entropy ({Hmeta})")

print("\n#node module meta")
for node in im.iterLeafNodes():
	print(f"{node.id} {node.moduleIndex()} {node.getMetaData()}")
	
