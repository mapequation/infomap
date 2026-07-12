from infomap import Network, Options

# Compare codelengths for two different partitions of a network
# composed of two triangles {0,1,2} and {5,6,7} connected by a
# chain of two nodes in the middle {3,4}.

net = Network()

# Add weight as an optional third argument
net.add_link(0, 1)
net.add_link(0, 2)
net.add_link(1, 2)
net.add_link(2, 3)
net.add_link(3, 4)
net.add_link(4, 5)
net.add_link(5, 6)
net.add_link(5, 7)
net.add_link(6, 7)

# Three modules, with the chain in its own module
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

# no_infomap skips optimization and just calculates the codelength
# of the given partition
opts = Options(two_level=True, no_infomap=True)

result = net.run(options=opts, initial_partition=partition1)

print(
    f"Partition one with {result.num_top_modules} modules -> "
    f"codelength: {result.codelength:.8f} bits"
)


result = net.run(options=opts, initial_partition=partition2)

print(
    f"Partition two with {result.num_top_modules} modules -> "
    f"codelength: {result.codelength:.8f} bits"
)

# Output:
# Partition one with 3 modules -> codelength: 2.55555556 bits
# Partition two with 2 modules -> codelength: 2.60715483 bits
