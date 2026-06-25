from __future__ import annotations

import math

import numpy as np
import pytest
from infomap import Infomap
from infomap.io.edge_index import add_edge_index


class FakeTensor:
    def __init__(self, value):
        self.value = np.asarray(value)

    def detach(self):
        return self

    def cpu(self):
        return self

    def numpy(self):
        return self.value


class IncompleteTensor:
    def detach(self):
        return self


class Recorder:
    flowModelIsSet = False

    @property
    def _core(self):
        return self

    def __init__(self):
        self.directed = False
        self.nodes = []
        self.links = []

    def setDirected(self, value):
        self.directed = value

    def add_node(self, node_id, name=None):
        self.nodes.append((node_id, name))

    def add_link(self, source_id, target_id, weight=1.0):
        self.links.append((source_id, target_id, weight))


def test_from_edge_index_accepts_numpy_array():
    edge_index = np.array([[0, 1, 1, 2], [1, 0, 2, 1]], dtype=np.int64)

    im = Infomap.from_edge_index(edge_index, directed=False, args="--silent")
    im.run()

    assert im.num_links == 2
    assert set(im.get_modules()) == {0, 1, 2}


def test_add_edge_index_accepts_list_input(make_infomap):
    im = make_infomap(no_infomap=True)

    mapping = im.add_edge_index([[0, 1], [1, 2]], directed=True)

    assert mapping == {0: 0, 1: 1, 2: 2}
    assert im.num_links == 2


def test_add_edge_index_accepts_torch_like_tensors():
    recorder = Recorder()

    mapping = add_edge_index(
        recorder,
        FakeTensor([[0, 1], [1, 2]]),
        edge_weight=FakeTensor([2.5, 3.5]),
    )

    assert mapping == {0: 0, 1: 1, 2: 2}
    assert recorder.links == [(0, 1, 2.5), (1, 2, 3.5)]


def test_add_edge_index_rejects_incomplete_tensor_interface(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match=r"detach\(\)\.cpu\(\)\.numpy\(\)"):
        im.add_edge_index(IncompleteTensor())


def test_add_edge_index_defaults_to_unweighted_edges():
    recorder = Recorder()

    add_edge_index(recorder, np.array([[0, 1], [1, 2]], dtype=np.int64))

    assert recorder.links == [(0, 1, 1.0), (1, 2, 1.0)]


def test_add_edge_index_rejects_invalid_edge_weight_length(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="one value per edge"):
        im.add_edge_index([[0, 1], [1, 2]], edge_weight=[1.0])


def test_add_edge_index_rejects_nan_edge_weight(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="NaN"):
        im.add_edge_index([[0], [1]], edge_weight=[math.nan])


def test_add_edge_index_rejects_negative_edge_weight(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="negative"):
        im.add_edge_index([[0], [1]], edge_weight=[-1.0])


def test_add_edge_index_sets_directed_by_default():
    recorder = Recorder()

    add_edge_index(recorder, np.array([[0], [1]], dtype=np.int64))

    assert recorder.directed is True


def test_add_edge_index_undirected_deduplicates_reverse_edges():
    recorder = Recorder()

    add_edge_index(
        recorder,
        np.array([[0, 1, 1, 2], [1, 0, 2, 1]], dtype=np.int64),
        edge_weight=[1.0, 2.0, 3.0, 4.0],
        directed=False,
    )

    assert recorder.directed is False
    assert recorder.links == [(0, 1, 2.0), (1, 2, 4.0)]


def test_from_edge_index_preserves_isolated_nodes_with_num_nodes():
    im = Infomap.from_edge_index([[0], [1]], num_nodes=4, args="--silent")

    im.run()

    assert set(im.get_modules()) == {0, 1, 2, 3}


def test_add_edge_index_returns_custom_node_id_mapping(make_infomap):
    im = make_infomap(no_infomap=True)

    mapping = im.add_edge_index(
        [[0, 1], [1, 2]],
        node_ids=["gene_a", "gene_b", "gene_c"],
    )
    im.run()
    modules = {
        mapping[node_id]: module_id for node_id, module_id in im.get_modules().items()
    }

    assert mapping == {0: "gene_a", 1: "gene_b", 2: "gene_c"}
    assert dict(im.names) == {0: "gene_a", 1: "gene_b", 2: "gene_c"}
    assert set(modules) == {"gene_a", "gene_b", "gene_c"}


def test_add_edge_index_rejects_invalid_shape(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match=r"shape `\(2, num_edges\)`"):
        im.add_edge_index([[0, 1], [1, 2], [2, 3]])


def test_add_edge_index_rejects_negative_node_id(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="negative node ids"):
        im.add_edge_index([[0, -1], [1, 2]])


def test_add_edge_index_rejects_non_integer_node_id(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="integer node ids"):
        im.add_edge_index(np.array([[0.0], [1.0]]))


def test_add_edge_index_rejects_num_nodes_too_small(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="num_nodes"):
        im.add_edge_index([[0, 2], [1, 3]], num_nodes=3)


def test_add_edge_index_rejects_invalid_node_ids(make_infomap):
    im = make_infomap(no_infomap=True)

    with pytest.raises(ValueError, match="node_ids"):
        im.add_edge_index([[0], [1]], node_ids=["a"])

    with pytest.raises(ValueError, match="unique"):
        im.add_edge_index([[0], [1]], node_ids=["a", "a"])
