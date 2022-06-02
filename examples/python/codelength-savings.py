from infomap import Infomap

im = Infomap()
im.read_file("../networks/ninetriangles.net")
im.run(num_trials=5)
#im.write("../../ninetriangles.json")

print(f"Found {im.num_levels} levels with codelength {im.codelength:.8f} bits")

s = {}

L = 0.0
index = 0.0

print("path\t savings\t codelength")
for parent in im.tree:
    if parent.is_leaf:
        continue
    if parent.is_root:
        index = parent.codelength

    L += parent.codelength
    savings = 0.0
    codelength = 0.0

    for child in parent.testIter():
        if child.is_leaf:
            continue

        if child.depth == parent.depth + 1:
            savings += child.oneLevelCodelength()
            codelength += child.codelength

    print(parent.path, "\t", f"{parent.savings():.5f}\t", f"{parent.hierarchicalCodelength():.4f}")

print(index)
print(f"{L=}")
