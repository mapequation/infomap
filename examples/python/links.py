from infomap import Infomap
import os

network = """
*Vertices 5
1 "i"
2 "j"
3 "k"
4 "l"
5 "m"
*States
1 1
2 2
3 3
4 1
5 4
6 5
*Links
1 2 0.8
1 3 0.8
1 5 0.2
1 6 0.2
2 1 1
2 3 1
3 1 1
3 2 1
4 5 0.8
4 6 0.8
4 2 0.2
4 3 0.2
5 4 1
5 6 1
6 4 1
6 5 1
"""

filename = os.path.join("/tmp", "1nf0m4p.net")

with open(filename, "w") as f:
    f.write(network)

im = Infomap(silent=True)
im.read_file(filename)
im.run()

os.remove(filename)

print("source - target : weight")
for (source, target), weight in im.links.items():
    print(source, "-", target, ":", weight)

print("source - target : flow")
for (source, target), flow in im.flow_links.items():
    print(source, "-", target, ":", flow)
