from infomap import Infomap

im = Infomap(silent=True)
im.read_file("../networks/states.net")
result = im.run()

print("source target weight")
for source, target, weight in result.links():  # default data="weight"
    print(source, target, weight)

print("source target flow")
for source, target, flow in result.links(data="flow"):
    print(source, target, f"{flow:.4f}")
