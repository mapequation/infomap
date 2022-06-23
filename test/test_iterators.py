from infomap import Infomap


def test_iter_physical():
    im = Infomap(num_trials=10, silent=True)
    # im.read_file("test/data/ninetriangles.net")
    im.read_file("test/data/twotriangles.net")
    im.run()

    modules = sorted([(node.node_id, node.module_id) for node in im.iterLeafNodesPhysical()])
    assert modules == [(1, 1), (2, 1), (3, 1), (4, 2), (5, 2), (6, 2)]


if __name__ == "__main__":
    for count in range(100):
        print(f"============ Count: {count}")
        im = Infomap(num_trials=10, silent=True)
        # im.read_file("test/data/ninetriangles.net")
        # im.read_file("test/data/twotriangles.net")
        im.read_file("examples/networks/states.net")
        im.run()
        # for node in im.physical_nodes:
        #   print(node.node_id, node.module_id)

        print("Create iterator")
        it = im.iterLeafNodesPhysical2()
        # it = im.iterTreePhysical()
        print("Iterate physical nodes:")
        for node in it:
            if node.is_leaf:
                print(node.node_id, node.module_id)
        # print("Iterate state leaf nodes:")
        # for node in im.nodes:
        #     print(node.state_id, node.node_id, node.module_id)

        # it = infomap.InfomapLeafIteratorPhysical(im.root(), -1)
        # print(it.current.stateId)
