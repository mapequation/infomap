import infomap


im = infomap.Infomap("--two-level --silent")

# Add weight as an optional third argument
im.add_link(1, 2)
im.add_link(1, 3)
im.add_link(2, 3)
im.add_link(3, 4)
im.add_link(4, 5)
im.add_link(4, 6)
im.add_link(5, 6)

im.run()

print(f"Found {im.num_top_modules} modules with codelength: {im.codelength}")

modules = im.get_modules()

print("Modify the network and test partition...")

# Do some modification to the network
im.add_link(1, 5)
# Note that removing links will not remove nodes if they become unconnected
im.remove_link(5, 6)

# Run again with the optimal partition from the original network as initial solution
# Set no_infomap to skip optimization and just calculate the codelength
im.no_infomap = True
im.run(initial_partition=modules)

print(f"Found {im.num_top_modules} modules with codelength: {im.codelength}")
