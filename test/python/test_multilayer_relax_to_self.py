import pytest


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
