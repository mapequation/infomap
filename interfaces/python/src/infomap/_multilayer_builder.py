"""Multilayer network construction for an :class:`infomap.Infomap` instance.

Mirrors :class:`infomap._network_builder._NetworkBuilder`: holds a
back-reference to the Infomap instance and calls the SWIG multilayer
constructors through it. Behavior matches the former facade methods; bulk
inputs go straight to the C++ bulk constructors.
"""

from __future__ import annotations

from ._network_input import add_bulk_links as _add_bulk_links
from ._network_input import flat_multilayer_unpacker as _flat_multilayer_unpacker
from ._network_input import paired_multilayer_unpacker as _paired_multilayer_unpacker


class _MultilayerBuilder:
    def __init__(self, core):
        self._core = core

    def add_multilayer_link(
        self, source_multilayer_node, target_multilayer_node, weight=1.0
    ):
        source_layer_id, source_node_id = source_multilayer_node
        target_layer_id, target_node_id = target_multilayer_node
        return self._core.addMultilayerLink(
            source_layer_id, source_node_id, target_layer_id, target_node_id, weight
        )

    def add_multilayer_intra_link(
        self, layer_id, source_node_id, target_node_id, weight=1.0
    ):
        return self._core.addMultilayerIntraLink(
            layer_id, source_node_id, target_node_id, weight
        )

    def add_multilayer_intra_links(self, links):
        return _add_bulk_links(
            links,
            numpy_method=self._core.addMultilayerIntraLinksFromNumpy2D,
            list_method=self._core.addMultilayerIntraLinks,
            name="multilayer intra-link",
            valid_columns=(3, 4),
            column_description="(layer_id, source_node_id, target_node_id, [weight])",
            valid_lengths=(3, 4),
            unpack=_flat_multilayer_unpacker(
                ("layer_id", "source_node_id", "target_node_id"),
            ),
            length_description="3 or 4 values",
        )

    def add_multilayer_inter_link(
        self, source_layer_id, node_id, target_layer_id, weight=1.0
    ):
        return self._core.addMultilayerInterLink(
            source_layer_id, node_id, target_layer_id, weight
        )

    def add_multilayer_inter_links(self, links):
        return _add_bulk_links(
            links,
            numpy_method=self._core.addMultilayerInterLinksFromNumpy2D,
            list_method=self._core.addMultilayerInterLinks,
            name="multilayer inter-link",
            valid_columns=(3, 4),
            column_description="(source_layer_id, node_id, target_layer_id, [weight])",
            valid_lengths=(3, 4),
            unpack=_flat_multilayer_unpacker(
                ("source_layer_id", "node_id", "target_layer_id"),
            ),
            length_description="3 or 4 values",
        )

    def add_multilayer_links(self, links):
        return _add_bulk_links(
            links,
            numpy_method=self._core.addMultilayerLinksFromNumpy2D,
            list_method=self._core.addMultilayerLinks,
            name="multilayer link",
            valid_columns=(4, 5),
            column_description=(
                "(source_layer_id, source_node_id, target_layer_id, "
                "target_node_id, [weight])"
            ),
            valid_lengths=(2, 3),
            unpack=_paired_multilayer_unpacker(),
            length_description="2 or 3 values",
        )

    def remove_multilayer_link(self):
        raise NotImplementedError(
            "Removing multilayer links is not supported by the Python API."
        )
