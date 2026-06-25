"""First-class network input builder.

:class:`Network` unifies the two former internal builders
(``_NetworkBuilder`` + ``_MultilayerBuilder``) into one public class. It owns a
:class:`infomap._core.Core` handle and exposes the core building verbs:
single-layer nodes/links, state nodes, names, the three multilayer link
families, meta data, the bipartite start id, file reading, and node/link counts.

It is pure input construction and validation -- it does not run Infomap or hold
results. :class:`infomap.Infomap` composes a :class:`Network` over its own
settings-configured ``Core`` (shared instance) and delegates its building verbs
to it, so ``im.add_link(...)`` keeps working unchanged.

Mutators return ``self`` for fluent chaining. Bulk inputs route straight to the
C++ bulk constructors through ``Core`` (list -> ``addLinks``, numpy ->
``addLinksFromNumpy2D``, and the multilayer equivalents) -- no per-element Python
loop is introduced.

A standalone ``Network()`` is a builder only for now; running it through the
functional entry point (``infomap.run(net)``) arrives in a later phase.
"""

from __future__ import annotations

from ._core import Core
from ._network_input import add_bulk_links as _add_bulk_links
from ._network_input import first_order_unpacker as _first_order_unpacker
from ._network_input import flat_multilayer_unpacker as _flat_multilayer_unpacker
from ._network_input import paired_multilayer_unpacker as _paired_multilayer_unpacker


class Network:
    """A first-class Infomap network input builder.

    Parameters
    ----------
    core : infomap._core.Core, optional
        The engine boundary to build onto. If ``None``, a default silent,
        no-file-output ``Core`` is created and owned by this ``Network``. When
        composed by :class:`infomap.Infomap`, the shared, settings-configured
        ``Core`` is passed in.
    """

    def __init__(self, core=None):
        if core is None:
            core = Core("--silent --no-file-output")
        self._core = core

    # ----------------------------------------
    # Input
    # ----------------------------------------

    def read_file(self, filename, accumulate=True):
        """Read network data from file.

        Parameters
        ----------
        filename : str
        accumulate : bool, optional
            If the network data should be accumulated to already added
            nodes and links. Default ``True``.
        """
        self._core.readInputData(filename, accumulate)
        return self

    # ----------------------------------------
    # Single-layer nodes
    # ----------------------------------------

    def add_node(self, node_id, name=None, teleportation_weight=None):
        """Add a node.

        Parameters
        ----------
        node_id : int
        name : str, optional
        teleportation_weight : float, optional
            Used for teleporting between layers in multilayer networks.
        """
        if name is None:
            name = ""

        if len(name) and teleportation_weight is not None:
            self._core.addNode(node_id, name, teleportation_weight)
        elif len(name) and teleportation_weight is None:
            self._core.addNode(node_id, name)
        elif not len(name) and teleportation_weight is not None:
            self._core.addNode(node_id, teleportation_weight)
        else:
            self._core.addNode(node_id)
        return self

    def add_nodes(self, nodes):
        """Add nodes.

        Parameters
        ----------
        nodes : iterable of tuples or iterable of int or dict
            Iterable of tuples on the form
            ``(node_id, [name], [teleportation_weight])``.
        """
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
        return self

    def add_state_node(self, state_id, node_id):
        """Add a state node.

        Parameters
        ----------
        state_id : int
        node_id : int
            Id of the physical node the state node should be added to.
        """
        self._core.addStateNode(state_id, node_id)
        return self

    def add_state_nodes(self, state_nodes):
        """Add state nodes.

        Parameters
        ----------
        state_nodes : iterable of tuples or dict of int: int
            Iterable of tuples of the form ``(state_id, node_id)``
            or dict of the form ``{state_id: node_id}``.
        """
        try:
            for node in state_nodes.items():
                self.add_state_node(*node)
        except AttributeError:
            for node in state_nodes:
                self.add_state_node(*node)
        return self

    def set_name(self, node_id, name):
        """Set the name of a node.

        Parameters
        ----------
        node_id : int
        name : str
        """
        if name is None:
            name = ""
        self._core.addName(node_id, name)
        return self

    def set_names(self, names):
        """Set names to several nodes at once.

        Parameters
        ----------
        names : iterable of tuples or dict of int: str
            Iterable of tuples on the form ``(node_id, name)``
            or dict of the form ``{node_id: name}``.
        """
        try:
            for name in names.items():
                self.set_name(*name)
        except AttributeError:
            for name in names:
                self.set_name(*name)
        return self

    # ----------------------------------------
    # Single-layer links
    # ----------------------------------------

    def add_link(self, source_id, target_id, weight=1.0):
        """Add a link.

        Parameters
        ----------
        source_id : int
        target_id : int
        weight : float, optional
        """
        self._core.addLink(source_id, target_id, weight)
        return self

    def add_links(self, links):
        """Add several links.

        Parameters
        ----------
        links : iterable of tuples or numpy.ndarray
            Iterable of tuples of int of the form
            ``(source_id, target_id, [weight])``. NumPy arrays must be
            2-dimensional with 2 or 3 columns.
        """
        _add_bulk_links(
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
        return self

    def remove_link(self, source_id, target_id):
        """Remove a link.

        Parameters
        ----------
        source_id : int
        target_id : int
        """
        self._core.network().removeLink(source_id, target_id)
        return self

    def remove_links(self, links):
        """Remove several links.

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form ``(source_id, target_id)``.
        """
        for link in links:
            self.remove_link(*link)
        return self

    # ----------------------------------------
    # Multilayer links
    # ----------------------------------------

    def add_multilayer_link(
        self, source_multilayer_node, target_multilayer_node, weight=1.0
    ):
        """Add a multilayer link.

        Parameters
        ----------
        source_multilayer_node : tuple of int, or MultilayerNode
            ``(layer_id, node_id)``.
        target_multilayer_node : tuple of int, or MultilayerNode
            ``(layer_id, node_id)``.
        weight : float, optional
        """
        source_layer_id, source_node_id = source_multilayer_node
        target_layer_id, target_node_id = target_multilayer_node
        self._core.addMultilayerLink(
            source_layer_id, source_node_id, target_layer_id, target_node_id, weight
        )
        return self

    def add_multilayer_links(self, links):
        """Add several multilayer links.

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form
            ``(source_node, target_node, [weight])``. NumPy arrays must be
            2-dimensional with 4 or 5 columns of the form
            ``(source_layer_id, source_node_id, target_layer_id,
            target_node_id, [weight])``.
        """
        _add_bulk_links(
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
        return self

    def add_multilayer_intra_link(
        self, layer_id, source_node_id, target_node_id, weight=1.0
    ):
        """Add an intra-layer link.

        Parameters
        ----------
        layer_id : int
        source_node_id : int
        target_node_id : int
        weight : float, optional
        """
        self._core.addMultilayerIntraLink(
            layer_id, source_node_id, target_node_id, weight
        )
        return self

    def add_multilayer_intra_links(self, links):
        """Add several intra-layer links.

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form
            ``(layer_id, source_node_id, target_node_id, [weight])``.
            NumPy arrays must be 2-dimensional with 3 or 4 columns.
        """
        _add_bulk_links(
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
        return self

    def add_multilayer_inter_link(
        self, source_layer_id, node_id, target_layer_id, weight=1.0
    ):
        """Add an inter-layer link.

        Parameters
        ----------
        source_layer_id : int
        node_id : int
        target_layer_id : int
        weight : float, optional
        """
        self._core.addMultilayerInterLink(
            source_layer_id, node_id, target_layer_id, weight
        )
        return self

    def add_multilayer_inter_links(self, links):
        """Add several inter-layer links.

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form
            ``(source_layer_id, node_id, target_layer_id, [weight])``.
            NumPy arrays must be 2-dimensional with 3 or 4 columns.
        """
        _add_bulk_links(
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
        return self

    def remove_multilayer_link(self):
        raise NotImplementedError(
            "Removing multilayer links is not supported by the Python API."
        )

    # ----------------------------------------
    # Meta data
    # ----------------------------------------

    def set_meta_data(self, node_id, meta_category):
        """Set meta data to a node.

        Parameters
        ----------
        node_id : int
        meta_category : int
        """
        self._core.network().addMetaData(node_id, meta_category)
        return self

    # ----------------------------------------
    # Properties
    # ----------------------------------------

    @property
    def bipartite_start_id(self):
        """Get or set the bipartite start id.

        Returns
        -------
        int
            The node id where the second node type starts.
        """
        return self._core.network().bipartiteStartId()

    @bipartite_start_id.setter
    def bipartite_start_id(self, start_id):
        self._core.setBipartiteStartId(start_id)

    @property
    def num_nodes(self):
        """The number of state nodes if a higher-order network, else physical.

        Returns
        -------
        int
            The number of nodes.
        """
        return self._core.network().numNodes()

    @property
    def num_links(self):
        """The number of links.

        Returns
        -------
        int
            The number of links.
        """
        return self._core.network().numLinks()
