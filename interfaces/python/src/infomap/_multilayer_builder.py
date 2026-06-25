"""Multilayer network construction for an :class:`infomap.Infomap` instance.

Mirrors :class:`infomap._network_builder._NetworkBuilder`: holds a
back-reference to the Infomap instance and calls the SWIG multilayer
constructors through it. Behavior matches the former facade methods; bulk
inputs go straight to the C++ bulk constructors.
"""

from __future__ import annotations

from ._network_input import flat_multilayer_unpacker as _flat_multilayer_unpacker
from ._network_input import is_numpy_input as _is_numpy_input
from ._network_input import normalize_numpy_links as _normalize_numpy_links
from ._network_input import paired_multilayer_unpacker as _paired_multilayer_unpacker
from ._network_input import split_optional_weight_rows as _split_optional_weight_rows


class _MultilayerBuilder:
    def __init__(self, im):
        self._im = im

    def add_multilayer_link(
        self, source_multilayer_node, target_multilayer_node, weight=1.0
    ):
        source_layer_id, source_node_id = source_multilayer_node
        target_layer_id, target_node_id = target_multilayer_node
        return self._im.addMultilayerLink(
            source_layer_id, source_node_id, target_layer_id, target_node_id, weight
        )

    def add_multilayer_intra_link(
        self, layer_id, source_node_id, target_node_id, weight=1.0
    ):
        return self._im.addMultilayerIntraLink(
            layer_id, source_node_id, target_node_id, weight
        )

    def add_multilayer_intra_links(self, links):
        if _is_numpy_input(links):
            links_array = _normalize_numpy_links(
                links,
                name="multilayer intra-link",
                valid_columns=(3, 4),
                column_description="(layer_id, source_node_id, target_node_id, [weight])",
            )
            return self._im.addMultilayerIntraLinksFromNumpy2D(
                links_array,
                links_array.shape[0],
                links_array.shape[1],
                links_array.dtype.kind,
                links_array.dtype.itemsize,
            )

        layer_ids, source_node_ids, target_node_ids, weights = (
            _split_optional_weight_rows(
                links,
                row_name="multilayer intra-link",
                valid_lengths=(3, 4),
                unpack=_flat_multilayer_unpacker(
                    ("layer_id", "source_node_id", "target_node_id"),
                ),
                length_description="3 or 4 values",
            )
        )

        return self._im.addMultilayerIntraLinks(
            layer_ids, source_node_ids, target_node_ids, weights
        )

    def add_multilayer_inter_link(
        self, source_layer_id, node_id, target_layer_id, weight=1.0
    ):
        return self._im.addMultilayerInterLink(
            source_layer_id, node_id, target_layer_id, weight
        )

    def add_multilayer_inter_links(self, links):
        if _is_numpy_input(links):
            links_array = _normalize_numpy_links(
                links,
                name="multilayer inter-link",
                valid_columns=(3, 4),
                column_description="(source_layer_id, node_id, target_layer_id, [weight])",
            )
            return self._im.addMultilayerInterLinksFromNumpy2D(
                links_array,
                links_array.shape[0],
                links_array.shape[1],
                links_array.dtype.kind,
                links_array.dtype.itemsize,
            )

        source_layer_ids, node_ids, target_layer_ids, weights = (
            _split_optional_weight_rows(
                links,
                row_name="multilayer inter-link",
                valid_lengths=(3, 4),
                unpack=_flat_multilayer_unpacker(
                    ("source_layer_id", "node_id", "target_layer_id"),
                ),
                length_description="3 or 4 values",
            )
        )

        return self._im.addMultilayerInterLinks(
            source_layer_ids, node_ids, target_layer_ids, weights
        )

    def add_multilayer_links(self, links):
        if _is_numpy_input(links):
            links_array = _normalize_numpy_links(
                links,
                name="multilayer link",
                valid_columns=(4, 5),
                column_description=(
                    "(source_layer_id, source_node_id, target_layer_id, "
                    "target_node_id, [weight])"
                ),
            )
            return self._im.addMultilayerLinksFromNumpy2D(
                links_array,
                links_array.shape[0],
                links_array.shape[1],
                links_array.dtype.kind,
                links_array.dtype.itemsize,
            )

        (
            source_layer_ids,
            source_node_ids,
            target_layer_ids,
            target_node_ids,
            weights,
        ) = _split_optional_weight_rows(
            links,
            row_name="multilayer link",
            valid_lengths=(2, 3),
            unpack=_paired_multilayer_unpacker(),
            length_description="2 or 3 values",
        )

        return self._im.addMultilayerLinks(
            source_layer_ids,
            source_node_ids,
            target_layer_ids,
            target_node_ids,
            weights,
        )

    def remove_multilayer_link(self):
        raise NotImplementedError(
            "Removing multilayer links is not supported by the Python API."
        )
