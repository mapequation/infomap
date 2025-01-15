import pathlib

from infomap import Infomap

im = Infomap(silent=False)


filename = "../../networks/db/science2001.net"

# You can read a network with the method read_file,
# which by default will accumulate to existing network data
im.read_file(filename)

print("Run Infomap...")
im.run(num_trials=5)

print(
    f"Found {im.max_depth} levels with {im.num_leaf_modules} leaf modules in {im.num_top_modules} top modules and codelength: {im.codelength:.8f} bits"
)
print(f"All codelengths: {im.codelengths}")

print("Done!")
