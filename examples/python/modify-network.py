from infomap import Network, Options

net = Network()

# Add weight as an optional third argument
net.add_link(1, 2)
net.add_link(1, 3)
net.add_link(2, 3)
net.add_link(3, 4)
net.add_link(4, 5)
net.add_link(4, 6)
net.add_link(5, 6)

result = net.run(options=Options(two_level=True))

print(
    f"Found {result.num_top_modules} modules with codelength "
    f"{result.codelength:.8f} bits"
)

modules = result.modules()

print("Modify the network and test partition...")

# Do some modification to the network
net.add_link(1, 5)
# Note that removing links will not remove nodes if they become unconnected
net.remove_link(5, 6)

# Run again with the optimal partition from the original network as initial
# solution. no_infomap skips optimization and just calculates the codelength.
result = net.run(
    options=Options(two_level=True, no_infomap=True), initial_partition=modules
)

print(
    f"Found {result.num_top_modules} modules with codelength "
    f"{result.codelength:.8f} bits"
)
