def _label_to_internal_id(labels):
    if not labels:
        return {}
    if isinstance(labels[0], int):
        return {label: label for label in labels}
    return {label: index for index, label in enumerate(labels)}


def _stable_unique_labels(labels):
    unique = []
    seen = set()
    for label in labels:
        if label in seen:
            continue
        seen.add(label)
        unique.append(label)
    return unique


def add_networkx_graph(
    infomap,
    g,
    *,
    weight="weight",
    phys_id="phys_id",
    layer_id="layer_id",
    multilayer_inter_intra_format=True,
):
    try:
        nodes = list(g.nodes)
        first = nodes[0]
    except IndexError:
        return {}

    if not infomap.flowModelIsSet and g.is_directed():
        infomap.setDirected(True)

    node_map = _label_to_internal_id(nodes)
    is_string_id = isinstance(first, str)
    phys_ids = dict(g.nodes.data(phys_id))
    layer_ids = dict(g.nodes.data(layer_id))
    is_state_network = None not in phys_ids.values()
    is_multilayer_network = None not in layer_ids.values()

    phys_map = {}
    if is_state_network:
        phys_labels = [phys_ids[node] for node in nodes]
        phys_map = _label_to_internal_id(_stable_unique_labels(phys_labels))

        if not isinstance(phys_labels[0], int):
            for label, node_id in phys_map.items():
                infomap.set_name(node_id, str(label))
        else:
            for label in phys_map:
                infomap.set_name(label, f"{label}")

        if is_multilayer_network:
            for state_label, data in g.nodes.data():
                infomap.network.add_multilayer_node(
                    node_map[state_label],
                    data[layer_id],
                    phys_map[data[phys_id]],
                    1.0,
                )
        else:
            for state_label in nodes:
                infomap.add_state_node(node_map[state_label], phys_map[phys_ids[state_label]])
    else:
        for node, name in g.nodes.data("name"):
            node_id = node_map[node]
            node_name = node if is_string_id else name
            infomap.add_node(node_id, name=node_name)

    if is_multilayer_network:
        for source, target, data in g.edges.data():
            source_state_id = node_map[source]
            target_state_id = node_map[target]
            source_layer_id = layer_ids[source]
            target_layer_id = layer_ids[target]
            source_node_id = phys_map[phys_ids[source]]
            target_node_id = phys_map[phys_ids[target]]
            edge_weight = data[weight] if weight is not None and weight in data else 1.0

            if multilayer_inter_intra_format:
                if source_layer_id == target_layer_id:
                    infomap.add_multilayer_intra_link(
                        source_layer_id,
                        source_node_id,
                        target_node_id,
                        edge_weight,
                    )
                else:
                    if source_node_id != target_node_id:
                        raise RuntimeError(
                            "Multilayer intra/inter format does not support 'diagonal' links. Use `multilayer_inter_intra_format=False`"
                        )
                    infomap.add_multilayer_inter_link(
                        source_layer_id,
                        source_node_id,
                        target_layer_id,
                        edge_weight,
                    )
            else:
                infomap.network.add_multilayer_state_link(
                    source_state_id,
                    source_layer_id,
                    source_node_id,
                    target_state_id,
                    target_layer_id,
                    target_node_id,
                    edge_weight,
                )
    else:
        for source, target, data in g.edges.data():
            edge_weight = data[weight] if weight is not None and weight in data else 1.0
            infomap.add_link(node_map[source], node_map[target], edge_weight)

    return {node_id: label for label, node_id in node_map.items()}
