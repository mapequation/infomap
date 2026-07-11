"""First-class network input builder.

:class:`Network` unifies the two former internal builders
(``_NetworkBuilder`` + ``_MultilayerBuilder``) into one public class. It owns a
:class:`infomap._core.Core` handle and exposes the core building verbs:
single-layer nodes/links, state nodes, names, the three multilayer link
families, metadata, the bipartite start id, file reading, and node/link counts.

:class:`infomap.Infomap` composes a :class:`Network` over its own
options-configured ``Core`` (shared instance) and delegates its building verbs
to it, so ``im.add_link(...)`` keeps working unchanged.

Mutators return ``self`` for fluent chaining. Bulk inputs route straight to the
C++ bulk constructors through ``Core`` (list -> ``addLinks``, numpy ->
``addLinksFromNumpy2D``, and the multilayer equivalents) -- no per-element Python
loop is introduced.

A :class:`Network` can be run directly via :func:`infomap.run` (or
:meth:`Network.run`): the ``Network`` owns its ``Core`` and provides the minimal
engine contract a :class:`infomap.result.Result` needs -- a mutable
``_generation`` token bumped on every run plus its ``_core`` handle -- so the
same :class:`~infomap.result.Result` construction works for a ``Network`` as for
an :class:`Infomap`.
"""

from __future__ import annotations

import os
import warnings
from typing import TYPE_CHECKING, Any

from ._core import Core, apply_initial_partition
from ._logging import engine_log_routing as _engine_log_routing
from ._logging import is_routed as _is_log_routed
from .errors import NetworkParseError, _translate_engine_errors
from .io.writers import _NetworkWritersMixin
from ._network_input import add_bulk_links as _add_bulk_links
from ._network_input import first_order_unpacker as _first_order_unpacker
from ._network_input import flat_multilayer_unpacker as _flat_multilayer_unpacker
from ._network_input import paired_multilayer_unpacker as _paired_multilayer_unpacker
from ._run import _UNSET

if TYPE_CHECKING:
    from collections.abc import Mapping

    from ._options import Options
    from .result import Result


class Network(_NetworkWritersMixin):
    """A first-class Infomap network input builder.

    Build a network with the fluent ``add_*``/``set_*`` verbs (each returns
    ``self``), then run it via :meth:`run` or the functional :func:`infomap.run`.
    Both return an immutable :class:`~infomap.Result`.

    ``Network`` is an in-memory builder: it runs silently and does not write
    output files (its engine is created with ``--silent --no-file-output``, and
    that cannot be turned off per run). Read results off the returned
    :class:`~infomap.Result`. If you need Infomap to write ``.tree``/``.clu``
    output files, use :class:`~infomap.Infomap` instead.

    Examples
    --------
    Build a small network and run it directly:

    >>> from infomap import Network
    >>> net = (
    ...     Network()
    ...     .add_link(1, 2)
    ...     .add_link(1, 3)
    ...     .add_link(2, 3)
    ...     .add_link(3, 4)
    ...     .add_link(4, 5)
    ...     .add_link(4, 6)
    ...     .add_link(5, 6)
    ... )
    >>> result = net.run()
    >>> result.num_top_modules
    2
    >>> for node_id, module_id in sorted(result.modules().items()):
    ...     print(node_id, module_id)
    1 1
    2 1
    3 1
    4 2
    5 2
    6 2

    A :class:`Network` can also be handed to :func:`infomap.run`:

    >>> from infomap import run
    >>> result = run(net)
    >>> result.num_top_modules
    2

    Parameters
    ----------
    core : optional
        Internal engine handle to build onto. If ``None``, a default silent,
        no-file-output ``Core`` is created and owned by this ``Network``. When
        composed by :class:`infomap.Infomap`, the shared, options-configured
        ``Core`` is passed in.
    """

    #: ``{internal_id: label}`` mapping populated by the graph-library
    #: constructors (``from_networkx``/``from_igraph``/``from_scipy_sparse_matrix``/
    #: ``from_edge_index``). Empty on a manually built or file-loaded ``Network``,
    #: mirroring :class:`~infomap.Infomap`, so the attribute is always present.
    node_id_to_label: dict[int, Any]

    def __init__(self, core: Core | None = None) -> None:
        if core is None:
            core = Core("--silent --no-file-output")
        self._core = core
        # Always present so attribute access never raises; the graph-library
        # constructors replace it with the id->label mapping they build.
        self.node_id_to_label = {}
        # Run-generation token: incremented on every run(). A Result stamps the
        # generation it was created in; node-level access on a stale Result
        # (engine re-ran since) raises instead of reading freed memory. This is
        # the minimal engine contract Result needs, mirroring Infomap.
        self._generation = 0

    # ----------------------------------------
    # Constructors from graph libraries
    # ----------------------------------------

    @classmethod
    def from_file(
        cls, path: str | os.PathLike[str], *, accumulate: bool = True
    ) -> Network:
        """Build a :class:`Network` by reading a network file.

        Parameters
        ----------
        path : str or os.PathLike
            Path to an Infomap-readable network file.
        accumulate : bool, optional
            Accumulate onto already added nodes and links. Default ``True``.
        """
        net = cls()
        net.read_file(str(path), accumulate=accumulate)
        return net

    @classmethod
    def from_networkx(
        cls,
        g: Any,
        *,
        weight: str | None = "weight",
        node_id: str = "node_id",
        layer_id: str = "layer_id",
        multilayer_inter_intra_format: bool = True,
        meta_attribute: str | None = None,
    ) -> Network:
        """Build a :class:`Network` from a NetworkX graph.

        Loads ``g`` via the same adapter :meth:`Infomap.add_networkx_graph`
        uses. The ``{internal_id: label}`` mapping is stored on
        :attr:`node_id_to_label`. Pass ``meta_attribute`` to use a node
        attribute as Infomap metadata (values are encoded to integers, so
        string categories work).
        """
        from .io.networkx import add_networkx_graph as _add_networkx_graph

        net = cls()
        net.node_id_to_label = _add_networkx_graph(
            net,
            g,
            weight=weight,
            node_id=node_id,
            layer_id=layer_id,
            multilayer_inter_intra_format=multilayer_inter_intra_format,
            meta_attribute=meta_attribute,
        )
        return net

    @classmethod
    def from_igraph(
        cls,
        g: Any,
        *,
        edge_weights: Any = None,
        vertex_weights: Any = None,
        node_id: str = "node_id",
        layer_id: str = "layer_id",
        multilayer_inter_intra_format: bool = True,
        meta_attribute: str | None = None,
    ) -> Network:
        """Build a :class:`Network` from a python-igraph graph.

        Loads ``g`` via the same adapter :meth:`Infomap.add_igraph_graph` uses.
        The ``{internal_id: label}`` mapping is stored on
        :attr:`node_id_to_label`. Pass ``meta_attribute`` to use a vertex
        attribute as Infomap metadata (values are encoded to integers, so
        string categories work).
        """
        from .io.igraph import add_igraph_graph as _add_igraph_graph

        net = cls()
        net.node_id_to_label = _add_igraph_graph(
            net,
            g,
            edge_weights=edge_weights,
            vertex_weights=vertex_weights,
            node_id=node_id,
            layer_id=layer_id,
            multilayer_inter_intra_format=multilayer_inter_intra_format,
            meta_attribute=meta_attribute,
        )
        return net

    @classmethod
    def from_scipy_sparse_matrix(
        cls,
        A: Any,
        *,
        directed: bool = False,
        weighted: bool = True,
        node_ids: Any = None,
    ) -> Network:
        """Build a :class:`Network` from a SciPy sparse adjacency matrix.

        Loads ``A`` via the same adapter
        :meth:`Infomap.add_scipy_sparse_matrix` uses. The
        ``{internal_id: label}`` mapping is stored on
        :attr:`node_id_to_label`.
        """
        from .io.scipy import add_scipy_sparse_matrix as _add_scipy_sparse_matrix

        net = cls()
        net.node_id_to_label = _add_scipy_sparse_matrix(
            net,
            A,
            directed=directed,
            weighted=weighted,
            node_ids=node_ids,
        )
        return net

    @classmethod
    def from_edge_index(
        cls,
        edge_index: Any,
        *,
        edge_weight: Any = None,
        num_nodes: int | None = None,
        directed: bool = True,
        node_ids: Any = None,
    ) -> Network:
        """Build a :class:`Network` from a PyG-style edge index.

        Loads ``edge_index`` via the same adapter
        :meth:`Infomap.add_edge_index` uses. The ``{internal_id: label}``
        mapping is stored on :attr:`node_id_to_label`.
        """
        from .io.edge_index import add_edge_index as _add_edge_index

        net = cls()
        net.node_id_to_label = _add_edge_index(
            net,
            edge_index,
            edge_weight=edge_weight,
            num_nodes=num_nodes,
            directed=directed,
            node_ids=node_ids,
        )
        return net

    # ----------------------------------------
    # Run
    # ----------------------------------------

    def run(
        self,
        *,
        options: Options | Mapping[str, Any] | None = None,
        seed: int = _UNSET,
        num_trials: int = _UNSET,
        two_level: bool = _UNSET,
        directed: bool | None = _UNSET,
        markov_time: float = _UNSET,
        args: str | None = None,
        initial_partition: Mapping[Any, Any] | None = None,
        **overrides: Any,
    ) -> Result:
        """Run Infomap on this network and return a :class:`Result`.

        This is a thin convenience wrapper: ``net.run(**kw)`` is equivalent to
        ``infomap.run(net, **kw)``, the canonical entry point. It accepts the
        same keywords.

        Parameters
        ----------
        options : Options, mapping, or None, optional
            Configuration rendered to Infomap CLI flags for this run.
        seed, num_trials, two_level, directed, markov_time : optional
            The five common-tier engine options, accepted directly here to match
            :func:`infomap.run` and :meth:`infomap.Infomap.run`. A supplied value
            overrides the ``options`` carrier; carry any other engine option via
            ``options=Options(...)``.
        args : str, optional
            Raw Infomap arguments prepended before the rendered options.
        initial_partition : mapping, optional
            Initial module assignment for this run only, keyed by internal node
            id (``{node_or_state_id: module_id}``) or, for a multilayer network,
            ``{(layer_id, node_id): module_id}``. The internal ids are the ones
            you built the network with (the values of :attr:`node_id_to_label`).
        **overrides
            Any other Infomap engine option, as a keyword argument, forwarded
            to :class:`Options` and matching :func:`infomap.run`. Convenient for
            a one-off; for a reusable or validated configuration prefer
            ``options=Options(...)``, the canonical carrier and full reference.

        Returns
        -------
        Result
            An immutable snapshot of the run, bound to this ``Network``.

        Notes
        -----
        A ``Network`` engine is silent for its whole lifetime; ``silent=False``
        cannot re-enable the engine log here and warns instead. For the log,
        run through the stateful :class:`~infomap.Infomap` (constructed with
        ``silent=False``) or pass the input directly to :func:`infomap.run`.
        """
        from ._options import Options, _construct_args
        from ._run import _warn_inert_output_options
        from .result import build_result

        if options is None:
            resolved = Options()
        elif isinstance(options, Options):
            resolved = options
        else:
            resolved = Options(**dict(options))

        # Advanced engine options passed as bare keywords forward to Options
        # (the canonical carrier), matching infomap.run(); this convenience
        # front door does not deprecate them.
        # The five common-tier keywords mirror infomap.run()/Infomap.run(): a
        # supplied value overrides the options carrier; an unset one defers to
        # it. Advanced-tier overrides win over the carrier too. replace()
        # re-validates through Options.__post_init__ (unknown key -> TypeError
        # with a suggestion, out-of-range -> ValueError).
        common = {
            name: value
            for name, value in (
                ("seed", seed),
                ("num_trials", num_trials),
                ("two_level", two_level),
                ("directed", directed),
                ("markov_time", markov_time),
            )
            if value is not _UNSET
        }
        if common or overrides:
            resolved = resolved.replace(**{**common, **overrides})

        if _is_log_routed():
            # enable_log()/handlers on the "infomap" logger route the engine
            # log, but a Network's Core is constructed --silent for its whole
            # lifetime and that cannot be undone per run (the engine composes
            # constructor + run args additively), so a routed run of a Network
            # emits no records. Point the caller at a surface that can emit,
            # mirroring the stale-silent advisory on the Infomap path.
            warnings.warn(
                "the 'infomap' logger has handlers, but a Network engine is "
                "silent for its whole lifetime, so this run emits no log "
                "records. For the engine log, run through Infomap(...) or pass "
                "the input directly to infomap.run() (an edge list, graph, or "
                "file path) instead.",
                UserWarning,
                stacklevel=2,
            )
        elif resolved.silent is False:
            warnings.warn(
                "A Network engine is silent for its whole lifetime; "
                "silent=False has no effect here. For the engine log, run "
                "through Infomap(silent=False) or pass the input directly "
                "to infomap.run().",
                UserWarning,
                stacklevel=2,
            )

        _warn_inert_output_options(resolved, args)

        rendered_args = _construct_args(args, **resolved.to_kwargs())
        # classify=True mirrors Infomap.run(): input failures surfacing at
        # run time (cluster_data, meta_data) become NetworkParseError.
        if initial_partition is None:
            with _engine_log_routing(), _translate_engine_errors(classify=True):
                self._core.run(rendered_args)
        else:
            apply_initial_partition(self._core, initial_partition)
            try:
                with _engine_log_routing(), _translate_engine_errors(classify=True):
                    self._core.run(rendered_args)
            finally:
                # Applies to this run only, mirroring Infomap.run().
                apply_initial_partition(self._core, {})
        return build_result(self)

    # ----------------------------------------
    # Input
    # ----------------------------------------

    def read_file(self, filename: str, accumulate: bool = True) -> Network:
        """Read network data from file.

        Parameters
        ----------
        filename : str
        accumulate : bool, optional
            If the network data should be accumulated to already added
            nodes and links. Default ``True``.

        Raises
        ------
        NetworkParseError
            If the file cannot be opened or its content cannot be parsed.
        """
        with _engine_log_routing(), _translate_engine_errors(NetworkParseError):
            self._core.readInputData(filename, accumulate)
        return self

    # ----------------------------------------
    # Single-layer nodes
    # ----------------------------------------

    def add_node(
        self,
        node_id: int,
        name: str | None = None,
        teleportation_weight: float | None = None,
    ) -> Network:
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

    def add_nodes(self, nodes: Any) -> Network:
        """Add nodes.

        Parameters
        ----------
        nodes : iterable of tuples or iterable of int or dict
            Iterable of tuples on the form
            ``(node_id, [name], [teleportation_weight])``.
        """
        # Gate on the mapping protocol instead of catching AttributeError
        # around the loop: an AttributeError raised mid-iteration inside the
        # body would otherwise fall through and re-process earlier items.
        if hasattr(nodes, "items"):
            for node, attr in nodes.items():
                if isinstance(attr, str):
                    self.add_node(node, attr)
                else:
                    self.add_node(node, *attr)
        else:
            for node in nodes:
                if isinstance(node, int):
                    self.add_node(node)
                else:
                    self.add_node(*node)
        return self

    def add_state_node(self, state_id: int, node_id: int) -> Network:
        """Add a state node.

        Parameters
        ----------
        state_id : int
        node_id : int
            Id of the physical node the state node should be added to.
        """
        self._core.addStateNode(state_id, node_id)
        return self

    def add_state_nodes(self, state_nodes: Any) -> Network:
        """Add state nodes.

        Parameters
        ----------
        state_nodes : iterable of tuples or dict of int: int
            Iterable of tuples of the form ``(state_id, node_id)``
            or dict of the form ``{state_id: node_id}``.
        """
        # Gate on the mapping protocol instead of catching AttributeError
        # around the loop: an AttributeError raised mid-iteration inside the
        # body would otherwise fall through and re-process earlier items.
        if hasattr(state_nodes, "items"):
            for node in state_nodes.items():
                self.add_state_node(*node)
        else:
            for node in state_nodes:
                self.add_state_node(*node)
        return self

    def set_name(self, node_id: int, name: str | None) -> Network:
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

    def set_names(self, names: Any) -> Network:
        """Set names to several nodes at once.

        Parameters
        ----------
        names : iterable of tuples or dict of int: str
            Iterable of tuples on the form ``(node_id, name)``
            or dict of the form ``{node_id: name}``.
        """
        # Gate on the mapping protocol instead of catching AttributeError
        # around the loop: an AttributeError raised mid-iteration inside the
        # body would otherwise fall through and re-process earlier items.
        if hasattr(names, "items"):
            for name in names.items():
                self.set_name(*name)
        else:
            for name in names:
                self.set_name(*name)
        return self

    # ----------------------------------------
    # Single-layer links
    # ----------------------------------------

    def add_link(
        self, source_id: int, target_id: int, weight: float = 1.0
    ) -> Network:
        """Add a link.

        Parameters
        ----------
        source_id : int
        target_id : int
        weight : float, optional
        """
        self._core.addLink(source_id, target_id, weight)
        return self

    def add_links(self, links: Any) -> Network:
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

    def remove_link(self, source_id: int, target_id: int) -> Network:
        """Remove a link.

        Parameters
        ----------
        source_id : int
        target_id : int
        """
        self._core.network().removeLink(source_id, target_id)
        return self

    def remove_links(self, links: Any) -> Network:
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
        self,
        source_multilayer_node: Any,
        target_multilayer_node: Any,
        weight: float = 1.0,
    ) -> Network:
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

    def add_multilayer_links(self, links: Any) -> Network:
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
        self,
        layer_id: int,
        source_node_id: int,
        target_node_id: int,
        weight: float = 1.0,
    ) -> Network:
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

    def add_multilayer_intra_links(self, links: Any) -> Network:
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
        self,
        source_layer_id: int,
        node_id: int,
        target_layer_id: int,
        weight: float = 1.0,
    ) -> Network:
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

    def add_multilayer_inter_links(self, links: Any) -> Network:
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
        """Unsupported operation kept for interface parity.

        Raises
        ------
        NotImplementedError
            Always; removing multilayer links is not supported by the
            Python API. Rebuild the network without the link instead.
        """
        raise NotImplementedError(
            "Removing multilayer links is not supported by the Python API."
        )

    # ----------------------------------------
    # Metadata
    # ----------------------------------------

    def set_meta_data(
        self, node_id: Any, meta_category: int | None = None
    ) -> Network:
        """Set integer metadata for one node, or for many at once.

        Parameters
        ----------
        node_id : int or mapping
            A node id, or a ``{node_id: meta_category}`` mapping to assign
            metadata to several nodes in one call.
        meta_category : int, optional
            The meta category, when ``node_id`` is a single node id (ignored
            when ``node_id`` is a mapping).
        """
        if meta_category is None and hasattr(node_id, "items"):
            for nid, category in node_id.items():
                self._core.network().addMetaData(nid, category)
            return self
        self._core.network().addMetaData(node_id, meta_category)
        return self

    # ----------------------------------------
    # Properties
    # ----------------------------------------

    @property
    def network(self) -> Any:
        """The underlying SWIG state network.

        Exposed so the graph adapters (which reach for
        ``infomap.network.add_multilayer_node`` / ``add_multilayer_state_link``)
        work identically when building onto a :class:`Network` or an
        :class:`Infomap`.
        """
        return self._core.network()

    @property
    def bipartite_start_id(self) -> int:
        """Get or set the bipartite start id.

        Returns
        -------
        int
            The node id where the second node type starts.
        """
        return self._core.network().bipartiteStartId()

    @bipartite_start_id.setter
    def bipartite_start_id(self, start_id: int) -> None:
        self._core.setBipartiteStartId(start_id)

    @property
    def num_nodes(self) -> int:
        """The number of state nodes if a higher-order network, else physical.

        Returns
        -------
        int
            The number of nodes.
        """
        return self._core.network().numNodes()

    @property
    def num_links(self) -> int:
        """The number of links.

        Returns
        -------
        int
            The number of links.
        """
        return self._core.network().numLinks()
