import pytest
import reference_flow
from infomap import Network, run

pytestmark = pytest.mark.fast


INTRA_LINKS = [
    (1, 1, 2, 1.0),
    (2, 1, 2, 1.0),
    (2, 1, 3, 1.0),
    (2, 1, 4, 1.0),
]


def test_relax_to_self_kwarg_accepted(make_infomap):
    # Raises TypeError if the facade signature was not updated, and the C++
    # parser would reject the rendered flag if the binding were not rebuilt.
    im = make_infomap(multilayer_relax_to_self=True)
    assert im is not None


def test_relax_to_self_produces_fewer_links_than_spread(make_infomap):
    spread = make_infomap(directed=True)
    for link in INTRA_LINKS:
        spread.add_multilayer_intra_link(*link)
    spread.run()

    aligned = make_infomap(directed=True, multilayer_relax_to_self=True)
    for link in INTRA_LINKS:
        aligned.add_multilayer_intra_link(*link)
    aligned.run()

    # Node-aligned coupling collapses the spread inter-layer links to a single
    # diagonal link per layer pair, so the state network has strictly fewer links.
    assert aligned.num_links < spread.num_links


def test_relax_to_self_recovers_spread_partition(make_infomap):
    # Two communities (1,2,3) and (4,5,6) present in two layers, weakly bridged.
    intra = []

    def both(layer, a, b):
        intra.extend([(layer, a, b, 1.0), (layer, b, a, 1.0)])

    for layer in (1, 2):
        for a, b in [(1, 2), (2, 3), (1, 3), (4, 5), (5, 6), (4, 6), (3, 4)]:
            both(layer, a, b)

    def modules(to_self):
        im = make_infomap(num_trials=5, multilayer_relax_to_self=to_self)
        for link in intra:
            im.add_multilayer_intra_link(*link)
        im.run()
        return im.num_top_modules

    spread = modules(False)
    assert spread == 2
    # The flow correction makes to-self reproduce spread's partition instead of
    # fragmenting or collapsing to a single module.
    assert modules(True) == spread


# --- directed flow against the spread model ----------------------------------
#
# --multilayer-relax-to-self promises "a smaller state network with the same
# flow as the default". The default (spread) state network is built here by
# hand from the layer links, and its flow is taken from the independent
# reference_flow oracle. The coverage map in #1224 (item 3) lists the
# --to-nodes and --recorded-teleportation branches of the directed relax-to-self
# flow as never run.

RELAX_RATE = 0.15

# Three layers, every node has out-links in its own layer; node 3 is missing
# from layer 3.
LAYERS = [
    (1, 1, 2, 1.0),
    (1, 2, 3, 2.0),
    (1, 3, 1, 1.0),
    (1, 3, 4, 0.5),
    (1, 4, 1, 1.0),
    (2, 1, 3, 1.0),
    (2, 3, 2, 1.0),
    (2, 2, 1, 3.0),
    (2, 2, 4, 1.0),
    (2, 4, 3, 2.0),
    (3, 4, 1, 1.0),
    (3, 1, 2, 2.0),
    (3, 2, 4, 1.0),
]
INTER = [(1, 1, 2, 0.5), (2, 1, 1, 0.5), (2, 4, 1, 1.0)]

DIRECTED_VARIANTS = {
    "unrecorded-to-links": {},
    "unrecorded-to-nodes": {"to_nodes": True},
    "recorded-to-links": {"recorded_teleportation": True},
    "recorded-to-nodes": {"recorded_teleportation": True, "to_nodes": True},
}


def _layer_out(intra):
    out = {}
    for layer, source, target, weight in intra:
        out.setdefault(layer, {}).setdefault(source, {})[target] = weight
    return out


def _spread_links(intra, inter=()):
    """State links of the default model, keyed by (layer, physical node).

    Simulated coupling: a state (l, n) follows an out-link of n in layer l with
    probability 1 - r, and with probability r first picks a layer m by n's
    out-weight there, then an out-link of n in m. Explicit inter-layer links
    keep the intra-layer weights and spread each inter-layer weight over n's
    out-links in the target layer; an inter-layer link to a layer where n has
    no out-links is dropped.
    """
    out = _layer_out(intra)
    strength = {
        layer: {n: sum(links.values()) for n, links in nodes.items()}
        for layer, nodes in out.items()
    }
    links = {}

    def add(source, target, weight):
        links[(source, target)] = links.get((source, target), 0.0) + weight

    if not inter:
        nodes = {(layer, n) for layer, n, *_ in intra} | {
            (layer, t) for layer, _, t, _ in intra
        }
        for layer, n in nodes:
            total = sum(strength[m].get(n, 0.0) for m in out)
            if total <= 0:
                continue
            for m in out:
                for target, weight in out[m].get(n, {}).items():
                    value = RELAX_RATE * weight / total
                    if m == layer:
                        value += (1 - RELAX_RATE) * weight / strength[layer][n]
                    add((layer, n), (m, target), value)
        return links
    for layer, source, target, weight in intra:
        add((layer, source), (layer, target), weight)
    for layer, n, target_layer, weight in inter:
        targets = out[target_layer].get(n, {})
        for target, intra_weight in targets.items():
            add(
                (layer, n),
                (target_layer, target),
                weight * intra_weight / strength[target_layer][n],
            )
    return links


def _state_flow(result):
    return {
        (node.layer_id, node.node_id): node.flow
        for node in result.tree(states=True)
        if node.is_leaf
    }


def _run_multilayer(intra, inter, *, to_self, **options):
    net = Network()
    for link in intra:
        net.add_multilayer_intra_link(*link)
    for link in inter:
        net.add_multilayer_inter_link(*link)
    return run(
        net,
        flow_model="directed",
        multilayer_relax_rate=RELAX_RATE,
        multilayer_relax_to_self=to_self,
        num_trials=1,
        seed=7,
        **options,
    )


def _assert_spread_flow(intra, inter, variant, *, to_self):
    options = DIRECTED_VARIANTS[variant]
    result = _run_multilayer(intra, inter, to_self=to_self, **options)
    got = _state_flow(result)
    links = _spread_links(intra, inter)
    expected = reference_flow.flow(
        got,
        [(s, t, w) for (s, t), w in links.items()],
        "directed",
        recorded=options.get("recorded_teleportation", False),
        to_nodes=options.get("to_nodes", False),
    ).node
    assert set(got) == set(expected)
    for state, value in expected.items():
        assert got[state] == pytest.approx(value, rel=1e-9, abs=1e-12), state


@pytest.mark.parametrize("to_self", [False, True], ids=["spread", "to-self"])
@pytest.mark.parametrize(
    "variant", [v for v in DIRECTED_VARIANTS if v != "recorded-to-links"]
)
@pytest.mark.parametrize("inter", [(), INTER], ids=["simulated", "inter-links"])
def test_directed_flow_matches_spread(inter, variant, to_self):
    _assert_spread_flow(LAYERS, inter, variant, to_self=to_self)


@pytest.mark.parametrize("inter", [(), INTER], ids=["simulated", "inter-links"])
def test_directed_recorded_to_links_spread(inter):
    _assert_spread_flow(LAYERS, inter, "recorded-to-links", to_self=False)


@pytest.mark.xfail(
    strict=True,
    reason="#1313: recorded teleportation to links lands by the compact "
    "network's in-weights, not spread's",
)
@pytest.mark.parametrize("inter", [(), INTER], ids=["simulated", "inter-links"])
def test_directed_recorded_to_links_to_self(inter):
    _assert_spread_flow(LAYERS, inter, "recorded-to-links", to_self=True)


ABSENT = [*INTER, (1, 3, 3, 0.3)]


def test_directed_inter_link_to_absent_node_spread():
    # Node 3 has no out-links in layer 3, so spread drops the inter-layer link.
    _assert_spread_flow(LAYERS, ABSENT, "unrecorded-to-links", to_self=False)


@pytest.mark.xfail(
    strict=True,
    reason="#1314: to-self keeps the link and loses the flow that crosses "
    "it, so node flow sums to less than 1",
)
def test_directed_inter_link_to_absent_node_to_self():
    _assert_spread_flow(LAYERS, ABSENT, "unrecorded-to-links", to_self=True)
