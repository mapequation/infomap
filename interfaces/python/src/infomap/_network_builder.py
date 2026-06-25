"""Single-layer network construction for an :class:`infomap.Infomap` instance.

The builder holds a back-reference to the Infomap instance and calls the SWIG
bulk/single constructors through it (``im.addNode``, ``im.addLink``,
``im.addLinks``, ``im.addLinksFromNumpy2D``, …). Behavior matches the former
facade methods exactly; only the call target changed from ``super().X`` to
``self._core.X``. Bulk inputs go straight to the C++ bulk constructors — no
per-element Python loop is introduced.
"""

from __future__ import annotations

from ._network_input import add_bulk_links as _add_bulk_links
from ._network_input import first_order_unpacker as _first_order_unpacker


class _NetworkBuilder:
    def __init__(self, core):
        self._core = core

    def add_node(self, node_id, name=None, teleportation_weight=None):
        if name is None:
            name = ""

        if len(name) and teleportation_weight is not None:
            return self._core.addNode(node_id, name, teleportation_weight)
        elif len(name) and teleportation_weight is None:
            return self._core.addNode(node_id, name)
        elif not len(name) and teleportation_weight is not None:
            return self._core.addNode(node_id, teleportation_weight)

        return self._core.addNode(node_id)

    def add_nodes(self, nodes):
        try:
            for node, attr in nodes.items():
                if isinstance(attr, str):
                    self.add_node(node, attr)
                else:
                    self.add_node(node, *attr)
        except AttributeError:
            for node in nodes:
                if isinstance(node, int):
                    self.add_node(node)
                else:
                    self.add_node(*node)

    def add_state_node(self, state_id, node_id):
        return self._core.addStateNode(state_id, node_id)

    def add_state_nodes(self, state_nodes):
        try:
            for node in state_nodes.items():
                self.add_state_node(*node)
        except AttributeError:
            for node in state_nodes:
                self.add_state_node(*node)

    def set_name(self, node_id, name):
        if name is None:
            name = ""
        return self._core.addName(node_id, name)

    def set_names(self, names):
        try:
            for name in names.items():
                self.set_name(*name)
        except AttributeError:
            for name in names:
                self.set_name(*name)

    def add_link(self, source_id, target_id, weight=1.0):
        return self._core.addLink(source_id, target_id, weight)

    def add_links(self, links):
        return _add_bulk_links(
            links,
            numpy_method=self._core.addLinksFromNumpy2D,
            list_method=self._core.addLinks,
            name="link",
            valid_columns=(2, 3),
            column_description="(source_id, target_id, [weight])",
            valid_lengths=(2, 3),
            unpack=_first_order_unpacker(),
            length_description="2 or 3 values",
            require_32_or_64_bit=True,
        )

    def remove_link(self, source_id, target_id):
        return self._core.network.removeLink(source_id, target_id)

    def remove_links(self, links):
        for link in links:
            self.remove_link(*link)

    def set_meta_data(self, node_id, meta_category):
        return self._core.network.addMetaData(node_id, meta_category)

    @property
    def bipartite_start_id(self):
        return self._core.network.bipartiteStartId()

    @bipartite_start_id.setter
    def bipartite_start_id(self, start_id):
        self._core.setBipartiteStartId(start_id)
