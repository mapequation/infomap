def test_matchable_multilayer_ids_with_python_read_file(make_infomap, example_network_path):
    im = make_infomap(no_infomap=True, matchable_multilayer_ids=3)
    im.read_file(str(example_network_path("multilayer.net")))
    im.run()

    nodes = sorted((node.state_id, node.node_id, node.layer_id) for node in im.nodes)

    assert im.num_nodes == 6
    assert im.num_physical_nodes == 5
    assert nodes == [
        (9, 1, 1),
        (10, 1, 2),
        (18, 2, 2),
        (26, 3, 2),
        (33, 4, 1),
        (41, 5, 1),
    ]
