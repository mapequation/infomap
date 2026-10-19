"""Multilayer input without inter-layer links: relax limits and relax by JSD.

Without inter-layer links Infomap simulates the coupling between layers, and
the state network it builds decides the flow. The coverage map in #1224
(item 4) lists ``--multilayer-relax-limit``, ``-up``, ``-down`` and
``--multilayer-relax-by-jsd`` as never run. The expectations here come from the
option texts and ``flow-models/temporal.md``, not from the generator:

* the default model: a state ``(l, n)`` follows an out-link of ``n`` in layer
  ``l`` with probability ``1 - r``, and with probability ``r`` picks a layer
  ``m`` by ``n``'s out-weight there, then an out-link of ``n`` in ``m``. A
  limit removes the layers more than that many layer ids away ("in each
  direction"); ``-up`` and ``-down`` cap the two directions, and the tighter of
  a symmetric and a directed limit applies;
* relax by JSD couples two layers in proportion to the Jensen-Shannon
  similarity of the node's out-link distributions there, and not at all when
  they share no out-neighbour.
"""

from __future__ import annotations

import math

import pytest
import reference_flow
from infomap import Network, run

pytestmark = pytest.mark.fast

RELAX_RATE = 0.15


def _layers(layer_ids):
    """A triangle with a chord in every layer, its weights varying by layer."""
    links = []
    for k, layer in enumerate(layer_ids):
        for source, target, weight in [
            (1, 2, 1.0),
            (2, 3, 1.0),
            (3, 1, 1.0),
            (1, 3, 1.0),
        ]:
            links.append((layer, source, target, weight + 0.5 * k))
        links.append((layer, 1, 4 + k % 2, 1.0))
    return links


def _run(intra, **options):
    net = Network()
    for link in intra:
        net.add_multilayer_intra_link(*link)
    return run(
        net,
        flow_model="directed",
        multilayer_relax_rate=RELAX_RATE,
        num_trials=1,
        seed=7,
        **options,
    )


def _state_links(result):
    states = {
        node.state_id: (node.layer_id, node.node_id)
        for node in result.tree(states=True)
        if node.is_leaf
    }
    return {
        (states[source], states[target]): weight
        for source, target, weight in result.links("weight")
    }


def _layer_pairs(links):
    return {(s[0], t[0]) for s, t in links if s[0] != t[0]}


def _within(limit, up, down):
    symmetric = math.inf if limit < 0 else limit
    up = symmetric if up < 0 else min(symmetric, up)
    down = symmetric if down < 0 else min(symmetric, down)
    return lambda source, target: (
        source - target <= down if source >= target else target - source <= up
    )


def _expected_links(intra, within):
    out = {}
    for layer, source, target, weight in intra:
        out.setdefault(layer, {}).setdefault(source, {})[target] = weight
    strength = {
        layer: {n: sum(t.values()) for n, t in nodes.items()}
        for layer, nodes in out.items()
    }
    links = {}
    for layer, nodes in out.items():
        for n in nodes:
            reachable = [m for m in out if within(layer, m)]
            total = sum(strength[m].get(n, 0.0) for m in reachable)
            for m in reachable:
                for target, weight in out[m].get(n, {}).items():
                    value = RELAX_RATE * weight / total
                    if m == layer:
                        value += (1 - RELAX_RATE) * weight / strength[layer][n]
                    key = ((layer, n), (m, target))
                    links[key] = links.get(key, 0.0) + value
    return links


LIMITS = {
    "none": {},
    "limit-0": {"multilayer_relax_limit": 0},
    "limit-1": {"multilayer_relax_limit": 1},
    "up-0": {"multilayer_relax_limit_up": 0},
    "down-0": {"multilayer_relax_limit_down": 0},
    "up-1-down-2": {"multilayer_relax_limit_up": 1, "multilayer_relax_limit_down": 2},
    "limit-2-up-1-down-0": {
        "multilayer_relax_limit": 2,
        "multilayer_relax_limit_up": 1,
        "multilayer_relax_limit_down": 0,
    },
}


# Layer ids with gaps: layers 1 and 7 are six ids apart in a network of four
# layers. Without a limit in a direction every layer in it is in reach.
GAPS = (1, 3, 4, 7)
CASES = [
    *(
        pytest.param((1, 2, 3, 4), limits, id=f"{limits}-contiguous")
        for limits in LIMITS
    ),
    *(pytest.param(GAPS, limits, id=f"{limits}-gaps") for limits in LIMITS),
]


@pytest.mark.parametrize(("layer_ids", "limits"), CASES)
def test_relax_limits_state_network(layer_ids, limits):
    options = LIMITS[limits]
    intra = _layers(layer_ids)
    within = _within(
        options.get("multilayer_relax_limit", -1),
        options.get("multilayer_relax_limit_up", -1),
        options.get("multilayer_relax_limit_down", -1),
    )
    result = _run(intra, **options)
    got = _state_links(result)
    expected = _expected_links(intra, within)
    assert set(got) == set(expected)
    for link, value in expected.items():
        assert got[link] == pytest.approx(value, rel=1e-12), link

    flow = reference_flow.flow(
        [], [(s, t, w) for (s, t), w in expected.items()], "directed"
    )
    for node in result.tree(states=True):
        if node.is_leaf:
            state = (node.layer_id, node.node_id)
            assert node.flow == pytest.approx(flow.node[state], rel=1e-9, abs=1e-12)


# --- relax by JSD ------------------------------------------------------------


def _jensen_shannon_similarity(first, second):
    """One minus the JS divergence (base 2), weighting each side by its out-weight."""
    total1, total2 = sum(first.values()), sum(second.values())
    pi1, pi2 = total1 / (total1 + total2), total2 / (total1 + total2)

    def entropy(p):
        return -sum(x * math.log2(x) for x in p if x > 0)

    p1 = {k: v / total1 for k, v in first.items()}
    p2 = {k: v / total2 for k, v in second.items()}
    mixed = [pi1 * p1.get(k, 0.0) + pi2 * p2.get(k, 0.0) for k in set(p1) | set(p2)]
    return 1 - (
        entropy(mixed) - pi1 * entropy(p1.values()) - pi2 * entropy(p2.values())
    )


# Node 1 has out-weight 3 in every layer, so the coupling to each layer can be
# compared without any assumption on how out-weight enters it. Layers 1 and 2
# agree on most of node 1's neighbourhood, layer 3 less, and layer 4 shares no
# out-neighbour with layer 1.
JSD_LAYERS = [
    (1, 1, 2, 1.0),
    (1, 1, 3, 2.0),
    (2, 1, 2, 1.5),
    (2, 1, 3, 1.5),
    (3, 1, 3, 1.0),
    (3, 1, 5, 2.0),
    (4, 1, 6, 3.0),
]
for _layer in (1, 2, 3, 4):
    JSD_LAYERS += [
        (_layer, 2, 1, 1.0),
        (_layer, 3, 1, 1.0),
        (_layer, 5, 1, 1.0),
        (_layer, 6, 1, 1.0),
    ]
NEIGHBOURS = {
    layer: {t: w for la, s, t, w in JSD_LAYERS if la == layer and s == 1}
    for layer in (1, 2, 3, 4)
}


def _relax_out(links, source):
    """Weight of the links from ``source`` into each other layer."""
    out = {}
    for (s, t), weight in links.items():
        if s == source and t[0] != source[0]:
            out[t[0]] = out.get(t[0], 0.0) + weight
    return out


def test_jsd_coupling_follows_similarity():
    links = _state_links(_run(JSD_LAYERS, multilayer_relax_by_jsd=True))
    for layer in (1, 2, 3):
        relax = _relax_out(links, (layer, 1))
        others = [m for m in (1, 2, 3) if m != layer]
        assert set(relax) == set(others), layer
        similarity = {
            m: _jensen_shannon_similarity(NEIGHBOURS[layer], NEIGHBOURS[m])
            for m in others
        }
        a, b = others
        assert relax[a] / relax[b] == pytest.approx(
            similarity[a] / similarity[b], rel=1e-12
        )
    # Layer 4 shares no out-neighbour of node 1 with the others.
    assert _relax_out(links, (4, 1)) == {}


JSD_LIMIT_LAYERS = _layers((1, 2, 3, 4))


@pytest.mark.xfail(
    strict=True,
    reason="#1323: relax by JSD reaches limit layers down but limit - 1 up",
)
@pytest.mark.parametrize("limit", [1, 2])
def test_jsd_relax_limit_is_symmetric(limit):
    result = _run(
        JSD_LIMIT_LAYERS, multilayer_relax_by_jsd=True, multilayer_relax_limit=limit
    )
    expected = {
        (a, b) for a in range(1, 5) for b in range(1, 5) if 0 < abs(a - b) <= limit
    }
    assert _layer_pairs(_state_links(result)) == expected


def test_jsd_without_limit_couples_all_layers():
    result = _run(JSD_LIMIT_LAYERS, multilayer_relax_by_jsd=True)
    expected = {(a, b) for a in range(1, 5) for b in range(1, 5) if a != b}
    assert _layer_pairs(_state_links(result)) == expected


@pytest.mark.xfail(
    strict=True,
    reason="#1323: relax by JSD with limit 0 drops the intra-layer links too",
)
def test_jsd_relax_limit_zero_keeps_intra_layer_links():
    result = _run(
        JSD_LIMIT_LAYERS, multilayer_relax_by_jsd=True, multilayer_relax_limit=0
    )
    links = _state_links(result)
    expected = {
        ((layer, source), (layer, target))
        for layer, source, target, _ in JSD_LIMIT_LAYERS
    }
    assert set(links) == expected


@pytest.mark.xfail(
    strict=True,
    reason="#1323: relax by JSD ignores --multilayer-relax-limit-up and -down",
)
@pytest.mark.parametrize(
    ("option", "expected"),
    [
        (
            "multilayer_relax_limit_up",
            {(a, b) for a in range(1, 5) for b in range(1, a)},
        ),
        (
            "multilayer_relax_limit_down",
            {(a, b) for a in range(1, 5) for b in range(a + 1, 5)},
        ),
    ],
    ids=["up-0", "down-0"],
)
def test_jsd_relax_limit_up_down(option, expected):
    result = _run(JSD_LIMIT_LAYERS, multilayer_relax_by_jsd=True, **{option: 0})
    assert _layer_pairs(_state_links(result)) == expected


def _transition(links):
    out = {}
    for (source, _), weight in links.items():
        out[source] = out.get(source, 0.0) + weight
    return {(s, t): w / out[s] for (s, t), w in links.items()}


SCALE_CASES = [
    pytest.param(False, id="default"),
    pytest.param(
        True,
        id="jsd",
        marks=pytest.mark.xfail(
            strict=True,
            reason="#1324: the JSD coupling grows with the link weights",
        ),
    ),
]


@pytest.mark.parametrize("jsd", SCALE_CASES)
def test_coupling_does_not_depend_on_weight_scale(jsd):
    # Multiplying every link weight by ten describes the same walk.
    def transition(scale):
        intra = [(la, s, t, w * scale) for la, s, t, w in JSD_LIMIT_LAYERS]
        return _transition(_state_links(_run(intra, multilayer_relax_by_jsd=jsd)))

    base, scaled = transition(1.0), transition(10.0)
    assert set(base) == set(scaled)
    for link, value in base.items():
        assert scaled[link] == pytest.approx(value, rel=1e-12), link
