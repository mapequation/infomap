import infomap

# Compare codelengths for two different partitions of a network
# composed of two triangles {0,1,2} and {5,6,7} connected by a
# chain of two nodes in the middle {3,4}.

im = infomap.Infomap("--two-level")

# Add weight as an optional third argument
im.add_link(0, 1)
im.add_link(0, 2)
im.add_link(1, 2)
im.add_link(2, 3)
im.add_link(3, 4)
im.add_link(4, 5)
im.add_link(5, 6)
im.add_link(5, 7)
im.add_link(6, 7)

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

im.set_initial_partition(partition1).run("--no-infomap")

print(f"Partition one with {im.num_top_modules} modules -> codelength: {im.codelength}")

im.set_initial_partition(partition2).run("--no-infomap")

print(f"Partition two with {im.num_top_modules} modules -> codelength: {im.codelength}")

# Output:
# Partition one with 3 modules -> codelength: 2.5555555555555554
# Partition two with 2 modules -> codelength: 2.60715482741224
