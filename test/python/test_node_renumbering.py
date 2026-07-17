import pytest

pytestmark = pytest.mark.fast


ISSUE_352_EDGES = [
    (1, 3, 1.0),
    (1, 4, 1.0),
    (1, 5, 1.0),
    (1, 6, 1.0),
    (1, 7, 1.0),
    (1, 8, 1.0),
    (2, 1, 1.0),
    (2, 4, 1.0),
    (4, 7, 1.0),
    (5, 4, 1.0),
    (5, 7, 1.0),
    (8, 1, 1.0),
    (8, 4, 1.0),
    (8, 6, 1.0),
    (8, 7, 1.0),
]

ISSUE_352_RELABEL = {
    1: 10,
    2: 80,
    3: 40,
    4: 70,
    5: 60,
    6: 30,
    7: 20,
    8: 50,
}


def _run_issue_352_graph(make_infomap, relabel=False):
    im = make_infomap(two_level=True, directed=True, seed=45, num_trials=3)
    for source_id, target_id, weight in ISSUE_352_EDGES:
        if relabel:
            source_id = ISSUE_352_RELABEL[source_id]
            target_id = ISSUE_352_RELABEL[target_id]
        im.add_link(source_id, target_id, weight)

    im.run()

    return im


def test_node_renumbering_preserves_codelength(make_infomap):
    base = _run_issue_352_graph(make_infomap)
    relabeled = _run_issue_352_graph(make_infomap, relabel=True)

    assert relabeled.codelength == pytest.approx(base.codelength, abs=1e-12)
