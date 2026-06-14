import sys
from collections import namedtuple
from contextlib import contextmanager
from html import escape

from ._bindings import *  # noqa: F401,F403
from ._bindings import __all__ as _BINDINGS_ALL
from ._edge_index import add_edge_index as _add_edge_index
from ._igraph import add_igraph_graph as _add_igraph_graph
from ._igraph import find_igraph_communities
from ._network_input import first_order_unpacker as _first_order_unpacker
from ._network_input import flat_multilayer_unpacker as _flat_multilayer_unpacker
from ._network_input import is_numpy_input as _is_numpy_input
from ._network_input import normalize_numpy_links as _normalize_numpy_links
from ._network_input import paired_multilayer_unpacker as _paired_multilayer_unpacker
from ._network_input import split_optional_weight_rows as _split_optional_weight_rows
from ._networkx import add_networkx_graph as _add_networkx_graph
from ._networkx import find_communities
from ._options import (
    InfomapOptions,
    _construct_args,
)
from ._results import _InfomapResultsMixin
from ._results import entropy, perplexity, plogp
from ._scipy import add_scipy_sparse_matrix as _add_scipy_sparse_matrix
from ._writers import _InfomapWritersMixin


def _package_construct_args():
    package_module = sys.modules.get(__package__)
    if package_module is None:
        return _construct_args
    return getattr(package_module, "_construct_args", _construct_args)


MultilayerNode = namedtuple("MultilayerNode", "layer_id, node_id")

_REPR_MAX_FLOW_FRACTION = 0.90
_REPR_MAX_MODULES = 12
_REPR_COLORS = (
    "#4477aa",
    "#ee6677",
    "#228833",
    "#ccbb44",
    "#66ccee",
    "#aa3377",
    "#bbbbbb",
    "#000000",
    "#994455",
    "#ddcc77",
    "#117733",
    "#88ccee",
)
_REPR_OTHER_COLOR = "#8f9499"


def _summary_data(im):
    data = {
        "nodes": im.num_nodes,
        "links": im.num_links,
        "physical_nodes": im.num_physical_nodes,
        "state_nodes": im.num_nodes - im.num_physical_nodes,
        "higher_order": im.num_nodes != im.num_physical_nodes,
        "status": "not run",
    }

    if im.num_top_modules == 0:
        return data

    data.update(
        {
            "status": "run",
            "state_input": im.stateInput,
            "multilayer_input": im.multilayerInput,
            "multilayer_network": im.isMultilayerNetwork(),
            "levels": im.num_levels,
            "leaf_nodes": im.numLeafNodes(),
            "top_modules": im.num_top_modules,
            "non_trivial_top_modules": im.num_non_trivial_top_modules,
            "leaf_modules": im.num_leaf_modules,
            "effective_top_modules": im.effective_num_top_modules,
            "effective_leaf_modules": im.effective_num_leaf_modules,
            "codelength": im.codelength,
            "index_codelength": im.index_codelength,
            "module_codelength": im.module_codelength,
            "one_level_codelength": im.one_level_codelength,
            "relative_codelength_savings": im.relative_codelength_savings,
            "entropy_rate": im.entropy_rate,
            "elapsed_time": im.elapsed_time,
        }
    )
    if im.meta_codelength != 0:
        data["meta_codelength"] = im.meta_codelength
    return data


def _format_summary_value(value):
    if isinstance(value, bool):
        return "Yes" if value else "No"
    if isinstance(value, float):
        return f"{value:.6g}"
    return str(value)


def _format_percent(value):
    return f"{100 * value:.3g}%"


def _top_module_flow_bars(im):
    modules = [
        (module.module_id, module.flow)
        for module in im.get_tree(depth_level=1)
        if module.depth == 1
    ]
    total_flow = sum(flow for _, flow in modules)
    if total_flow <= 0:
        return []

    bars = []
    cumulative_flow = 0.0
    for module_id, flow in sorted(modules, key=lambda item: item[1], reverse=True):
        if bars and (
            cumulative_flow >= _REPR_MAX_FLOW_FRACTION * total_flow
            or len(bars) >= _REPR_MAX_MODULES
        ):
            break
        bars.append(
            {
                "label": f"Module {module_id}",
                "flow": flow,
                "fraction": flow / total_flow,
                "color": _REPR_COLORS[len(bars) % len(_REPR_COLORS)],
            }
        )
        cumulative_flow += flow

    other_flow = total_flow - cumulative_flow
    if other_flow > 1e-12:
        bars.append(
            {
                "label": "Other modules",
                "flow": other_flow,
                "fraction": other_flow / total_flow,
                "color": _REPR_OTHER_COLOR,
            }
        )
    return bars


def _html_metric(label, value, *, value_class=""):
    return (
        '<div class="infomap-metric">'
        f'<span class="infomap-metric-label">{escape(label)}</span>'
        f'<span class="infomap-metric-value {escape(value_class)}">'
        f"{escape(_format_summary_value(value))}"
        "</span>"
        "</div>"
    )


def _html_flow_strip(bars):
    if not bars:
        return ""

    segments = []
    legend = []
    for bar in bars:
        label = escape(bar["label"])
        percent = escape(_format_percent(bar["fraction"]))
        color = escape(bar["color"])
        width = max(bar["fraction"] * 100, 0.25)
        segments.append(
            '<span class="infomap-flow-segment" '
            f'style="width: {width:.6g}%; background: {color};" '
            f'title="{label}: {percent}"></span>'
        )
        legend.append(
            '<span class="infomap-flow-item">'
            f'<span class="infomap-flow-swatch" style="background: {color};"></span>'
            f"{label} {percent}"
            "</span>"
        )

    return (
        '<div class="infomap-flow">'
        '<div class="infomap-section-label">Top-module flow</div>'
        f'<div class="infomap-flow-strip">{"".join(segments)}</div>'
        f'<div class="infomap-flow-legend">{"".join(legend)}</div>'
        "</div>"
    )


def _repr_html(im):
    summary = im.summary()
    is_run = summary["status"] == "run"
    status = "Run" if is_run else "Not run"
    status_class = "is-run" if is_run else "is-not-run"

    primary_fields = []
    secondary_fields = [
        ("Nodes", summary["nodes"]),
        ("Links", summary["links"]),
        ("Higher-order", summary["higher_order"]),
    ]
    if summary["physical_nodes"] != summary["nodes"]:
        secondary_fields.insert(1, ("Physical nodes", summary["physical_nodes"]))

    if is_run:
        primary_fields = [
            ("Codelength", summary["codelength"]),
            (
                "Relative savings",
                _format_percent(summary["relative_codelength_savings"]),
            ),
            ("Top modules", summary["top_modules"]),
            ("Effective modules", summary["effective_top_modules"]),
        ]
        secondary_fields.extend(
            [
                ("Levels", summary["levels"]),
                ("Elapsed time", f"{summary['elapsed_time']:.3g}s"),
            ]
        )

    metrics = [_html_metric("Status", status, value_class=status_class)]
    metrics.extend(_html_metric(label, value) for label, value in primary_fields)
    metrics.extend(_html_metric(label, value) for label, value in secondary_fields)
    flow_strip = _html_flow_strip(_top_module_flow_bars(im)) if is_run else ""

    return f"""
<style>
.infomap-card {{
  --infomap-border: color-mix(in srgb, currentColor 18%, transparent);
  --infomap-muted: color-mix(in srgb, currentColor 62%, transparent);
  --infomap-soft: color-mix(in srgb, currentColor 7%, transparent);
  border: 1px solid var(--infomap-border);
  border-radius: 8px;
  color: inherit;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  max-width: 720px;
  padding: 14px 16px 16px;
}}
.infomap-title {{
  font-size: 16px;
  font-weight: 650;
  line-height: 1.25;
  margin: 0 0 12px;
}}
.infomap-grid {{
  display: grid;
  gap: 8px;
  grid-template-columns: repeat(auto-fit, minmax(128px, 1fr));
}}
.infomap-metric {{
  background: var(--infomap-soft);
  border-radius: 6px;
  min-width: 0;
  padding: 8px 10px;
}}
.infomap-metric-label {{
  color: var(--infomap-muted);
  display: block;
  font-size: 11px;
  line-height: 1.25;
  margin-bottom: 3px;
}}
.infomap-metric-value {{
  display: block;
  font-size: 15px;
  font-weight: 620;
  line-height: 1.25;
  overflow-wrap: anywhere;
}}
.infomap-metric-value.is-run {{ color: #228833; }}
.infomap-metric-value.is-not-run {{ color: var(--infomap-muted); }}
.infomap-flow {{ margin-top: 14px; }}
.infomap-section-label {{
  color: var(--infomap-muted);
  font-size: 11px;
  margin-bottom: 6px;
}}
.infomap-flow-strip {{
  border-radius: 999px;
  display: flex;
  height: 12px;
  overflow: hidden;
  width: 100%;
}}
.infomap-flow-segment {{ display: block; min-width: 1px; }}
.infomap-flow-legend {{
  color: var(--infomap-muted);
  display: flex;
  flex-wrap: wrap;
  font-size: 11px;
  gap: 6px 12px;
  line-height: 1.3;
  margin-top: 8px;
}}
.infomap-flow-item {{ white-space: nowrap; }}
.infomap-flow-swatch {{
  border-radius: 999px;
  display: inline-block;
  height: 8px;
  margin-right: 4px;
  vertical-align: -1px;
  width: 8px;
}}
</style>
<div class="infomap-card">
  <div class="infomap-title">Infomap</div>
  <div class="infomap-grid">{"".join(metrics)}</div>
  {flow_strip}
</div>
"""


__all__ = [
    *_BINDINGS_ALL,
    "Infomap",
    "InfomapOptions",
    "MultilayerNode",
    "entropy",
    "find_communities",
    "find_igraph_communities",
    "main",
    "perplexity",
    "plogp",
]


class Infomap(_InfomapResultsMixin, _InfomapWritersMixin, InfomapWrapper):  # noqa: F405
    """Infomap

    This class is a thin wrapper around Infomap C++ compiled with SWIG.

    Examples
    --------
    Create an instance and add nodes and links:

    >>> from infomap import Infomap
    >>> im = Infomap(silent=True)
    >>> im.add_node(1)
    >>> im.add_node(2)
    >>> im.add_link(1, 2)
    >>> im.run()
    >>> im.codelength
    1.0


    Create an instance and read a network file:

    >>> from infomap import Infomap
    >>> im = Infomap(silent=True, num_trials=10)
    >>> im.read_file("ninetriangles.net")
    >>> im.run()
    >>> tol = 1e-4
    >>> abs(im.codelength - 3.4622731375264144) < tol
    True


    For more examples, see the examples directory.
    """

    def __init__(
        self,
        args=None,
        # input
        cluster_data=None,
        no_infomap=False,
        skip_adjust_bipartite_flow=False,
        bipartite_teleportation=False,
        weight_threshold=None,
        include_self_links=None,
        no_self_links=False,
        node_limit=None,
        matchable_multilayer_ids=None,
        assign_to_neighbouring_module=False,
        meta_data=None,
        meta_data_rate=1.0,
        meta_data_unweighted=False,
        # output
        tree=False,
        ftree=False,
        clu=False,
        verbosity_level=1,
        silent=False,
        pretty=False,
        out_name=None,
        no_file_output=False,
        clu_level=None,
        output=None,
        hide_bipartite_nodes=False,
        print_all_trials=False,
        no_overwrite=False,
        print_config_fingerprint=False,
        timing_json=None,
        summary_json=None,
        manifest_json=None,
        memory_report=False,
        # algorithm
        two_level=False,
        flow_model=None,
        directed=None,
        recorded_teleportation=False,
        use_node_weights_as_flow=False,
        to_nodes=False,
        teleportation_probability=0.15,
        regularized=False,
        regularization_strength=1.0,
        entropy_corrected=False,
        entropy_correction_strength=1.0,
        markov_time=1.0,
        variable_markov_time=False,
        variable_markov_damping=1.0,
        variable_markov_min_scale=1.0,
        preferred_number_of_modules=None,
        multilayer_relax_rate=0.15,
        multilayer_relax_limit=-1,
        multilayer_relax_limit_up=-1,
        multilayer_relax_limit_down=-1,
        multilayer_relax_by_jsd=False,
        # accuracy
        seed=123,
        num_trials=1,
        core_loop_limit=10,
        core_level_limit=None,
        tune_iteration_limit=None,
        core_loop_codelength_threshold=1e-10,
        tune_iteration_relative_threshold=1e-05,
        fast_hierarchical_solution=None,
        prefer_modular_solution=False,
        inner_parallelization=False,
        parallel_trials=False,
        num_threads=None,
        threads=None,
        trial_offset=None,
        trial_results=None,
        no_final_output=False,
        num_random_moves=None,
        max_degree_for_random_moves=None,
    ):
        """Create a new Infomap instance.

        Keyword arguments mirror the Infomap CLI flags. Use
        :class:`InfomapOptions` for a reusable configuration object and the full
        parameter reference.

        Parameters
        ----------
        args : str, optional
            Raw Infomap arguments to prepend before rendered keyword options.
        """
        options = InfomapOptions.from_mapping(locals())
        super().__init__(_package_construct_args()(args, **options.to_kwargs()))

    def __repr__(self):
        summary = _summary_data(self)
        fields = (
            "nodes",
            "links",
            "physical_nodes",
            "state_nodes",
            "status",
            "multilayer_network",
            "levels",
            "top_modules",
            "codelength",
        )
        details = [
            f"{field}={summary[field]!r}" for field in fields if field in summary
        ]
        return f"Infomap({', '.join(details)})"

    def summary(self):
        """Return a compact dictionary with network and result state.

        Before :meth:`run`, the summary contains loaded network counts and
        higher-order state-node information. After :meth:`run`, it also
        includes module counts, codelength components, entropy rate, and elapsed
        time.
        """
        return _summary_data(self)

    def _repr_html_(self):
        return _repr_html(self)

    @classmethod
    def from_options(cls, options, args=None):
        """Create an :class:`Infomap` instance from :class:`InfomapOptions`."""
        if not isinstance(options, InfomapOptions):
            raise TypeError("options must be an InfomapOptions instance")
        return cls(args=args, **options.to_kwargs())

    @classmethod
    def from_scipy_sparse_matrix(
        cls,
        A,
        *,
        directed=False,
        weighted=True,
        node_ids=None,
        args=None,
        **infomap_options,
    ):
        """Create an :class:`Infomap` instance from a SciPy sparse adjacency matrix."""
        im = cls(args=args, **infomap_options)
        im.add_scipy_sparse_matrix(
            A,
            directed=directed,
            weighted=weighted,
            node_ids=node_ids,
        )
        return im

    @classmethod
    def from_edge_index(
        cls,
        edge_index,
        *,
        edge_weight=None,
        num_nodes=None,
        directed=True,
        node_ids=None,
        args=None,
        **infomap_options,
    ):
        """Create an :class:`Infomap` instance from a PyG-style edge index."""
        im = cls(args=args, **infomap_options)
        im.add_edge_index(
            edge_index,
            edge_weight=edge_weight,
            num_nodes=num_nodes,
            directed=directed,
            node_ids=node_ids,
        )
        return im

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
        return super().readInputData(filename, accumulate)

    def add_node(self, node_id, name=None, teleportation_weight=None):
        """Add a node.

        See Also
        --------
        set_name
        add_nodes

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
            return super().addNode(node_id, name, teleportation_weight)
        elif len(name) and teleportation_weight is None:
            return super().addNode(node_id, name)
        elif not len(name) and teleportation_weight is not None:
            return super().addNode(node_id, teleportation_weight)

        return super().addNode(node_id)

    def add_nodes(self, nodes):
        """Add nodes.

        See Also
        --------
        add_node

        Examples
        --------
        Add nodes

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> im.add_nodes(range(4))


        Add named nodes

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> nodes = (
        ...     (1, "Node 1"),
        ...     (2, "Node 2"),
        ...     (3, "Node 3")
        ... )
        >>> im.add_nodes(nodes)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2', 3: 'Node 3'}


        Add named nodes with teleportation weights

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> nodes = (
        ...     (1, "Node 1", 0.5),
        ...     (2, "Node 2", 0.2),
        ...     (3, "Node 3", 0.8)
        ... )
        >>> im.add_nodes(nodes)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2', 3: 'Node 3'}

        Add named nodes using dict

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> nodes = {
        ...     1: "Node 1",
        ...     2: "Node 2",
        ...     3: "Node 3"
        ... }
        >>> im.add_nodes(nodes)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2', 3: 'Node 3'}


        Add named nodes with teleportation weights using dict

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> nodes = {
        ...     1: ("Node 1", 0.5),
        ...     2: ("Node 2", 0.2),
        ...     3: ("Node 3", 0.8)
        ... }
        >>> im.add_nodes(nodes)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2', 3: 'Node 3'}


        Parameters
        ----------
        nodes : iterable of tuples or iterable of int or dict of int: str or dict of int: tuple of (str, float)
            Iterable of tuples on the form
            ``(node_id, [name], [teleportation_weight])``
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

    def add_state_node(self, state_id, node_id):
        """Add a state node.

        Notes
        -----
        If a physical node with id node_id does not exist, it will be created.
        If you want to name the physical node, use ``set_name``.

        Parameters
        ----------
        state_id : int
        node_id : int
            Id of the physical node the state node should be added to.
        """
        return super().addStateNode(state_id, node_id)

    def add_state_nodes(self, state_nodes):
        """Add state nodes.

        See Also
        --------
        add_state_node

        Examples
        --------
        With tuples

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> states = (
        ...     (1, 1),
        ...     (2, 1),
        ...     (3, 2)
        ... )
        >>> im.add_state_nodes(states)


        With dict

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> states = {
        ...     1: 1,
        ...     2: 1,
        ...     3: 2
        ... }
        >>> im.add_state_nodes(states)


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

    def set_name(self, node_id, name):
        """Set the name of a node.

        Parameters
        ----------
        node_id : int
        name : str
        """
        if name is None:
            name = ""
        return super().addName(node_id, name)

    def set_names(self, names):
        """Set names to several nodes at once.

        Examples
        --------
        With tuples

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> names = (
        ...     (1, "Node 1"),
        ...     (2, "Node 2")
        ... )
        >>> im.set_names(names)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2'}


        With dict

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> names = {
        ...     1: "Node 1",
        ...     2: "Node 2"
        ... }
        >>> im.set_names(names)
        >>> im.names
        {1: 'Node 1', 2: 'Node 2'}


        See Also
        --------
        set_name, names

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

    def add_link(self, source_id, target_id, weight=1.0):
        """Add a link.

        Notes
        -----
        If the source or target nodes does not exist, they will be created.

        See Also
        --------
        remove_link

        Parameters
        ----------
        source_id : int
        target_id : int
        weight : float, optional
        """
        return super().addLink(source_id, target_id, weight)

    def add_links(self, links):
        """Add several links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> links = (
        ...     (1, 2),
        ...     (1, 3)
        ... )
        >>> im.add_links(links)
        >>> import numpy as np
        >>> im.add_links(np.array([[2, 3, 1.0], [3, 4, 2.0]]))


        See Also
        --------
        add_link
        remove_link

        Parameters
        ----------
        links : iterable of tuples or numpy.ndarray
            Iterable of tuples of int of the form
            ``(source_id, target_id, [weight])``. NumPy arrays must be
            2-dimensional with 2 or 3 columns, where the first two columns are
            source and target ids and the optional third column is link weight.
        """
        if _is_numpy_input(links):
            links_array = _normalize_numpy_links(
                links,
                name="link",
                valid_columns=(2, 3),
                column_description="(source_id, target_id, [weight])",
                require_32_or_64_bit=True,
            )
            return super().addLinksFromNumpy2D(
                links_array,
                links_array.shape[0],
                links_array.shape[1],
                links_array.dtype.kind,
                links_array.dtype.itemsize,
            )

        source_ids, target_ids, weights = _split_optional_weight_rows(
            links,
            row_name="link",
            valid_lengths=(2, 3),
            unpack=_first_order_unpacker(),
            length_description="2 or 3 values",
        )

        return super().addLinks(source_ids, target_ids, weights)

    def remove_link(self, source_id, target_id):
        """Remove a link.

        Notes
        -----
        Removing links will not remove nodes if they become disconnected.

        See Also
        --------
        add_link

        Parameters
        ----------
        source_id : int
        target_id : int
        """
        return self.network.removeLink(source_id, target_id)

    def remove_links(self, links):
        """Remove several links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> links = (
        ...     (1, 2),
        ...     (1, 3)
        ... )
        >>> im.add_links(links)
        >>> im.remove_links(links)
        >>> im.num_links
        0


        See Also
        --------
        remove_link

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form ``(source_id, target_id)``
        """
        for link in links:
            self.remove_link(*link)

    def add_multilayer_link(
        self, source_multilayer_node, target_multilayer_node, weight=1.0
    ):
        """Add a multilayer link.

        Adds a link between layers in a multilayer network.

        Examples
        --------
        Usage with tuples:

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> source_multilayer_node = (0, 1) # layer_id, node_id
        >>> target_multilayer_node = (1, 2) # layer_id, node_id
        >>> im.add_multilayer_link(source_multilayer_node, target_multilayer_node)


        Usage with MultilayerNode

        >>> from infomap import Infomap, MultilayerNode
        >>> im = Infomap()
        >>> source_multilayer_node = MultilayerNode(layer_id=0, node_id=1)
        >>> target_multilayer_node = MultilayerNode(layer_id=1, node_id=2)
        >>> im.add_multilayer_link(source_multilayer_node, target_multilayer_node)

        Notes
        -----
        This is the full multilayer format that supports both undirected
        and directed links. Infomap will not make any changes to the network.

        Parameters
        ----------
        source_multilayer_node : tuple of int, or MultilayerNode
            If passed a tuple, it should be of the format
            ``(layer_id, node_id)``.
        target_multilayer_node : tuple of int, or MultilayerNode
            If passed a tuple, it should be of the format
            ``(layer_id, node_id)``.
        weight : float, optional

        """
        source_layer_id, source_node_id = source_multilayer_node
        target_layer_id, target_node_id = target_multilayer_node
        return super().addMultilayerLink(
            source_layer_id, source_node_id, target_layer_id, target_node_id, weight
        )

    def add_multilayer_intra_link(
        self, layer_id, source_node_id, target_node_id, weight=1.0
    ):
        """Add an intra-layer link.

        Adds a link within a layer in a multilayer network.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> im.add_multilayer_intra_link(1, 1, 2)
        >>> im.add_multilayer_intra_link(1, 2, 3)
        >>> im.add_multilayer_intra_link(2, 1, 3)
        >>> im.add_multilayer_intra_link(2, 3, 4)

        Notes
        -----
        This multilayer format requires a directed network, so if
        the directed flag is not present, it will add all links
        also in their opposite direction to transform the undirected
        input to directed. If no inter-layer links are added, Infomap
        will simulate those by relaxing the random walker's constraint
        to its current layer. The final state network will be generated
        on run, which will clear the temporary data structure that holds
        the provided intra-layer links.

        Parameters
        ----------
        layer_id : int
        source_node_id : int
        target_node_id : int
        weight : float, optional


        """
        return super().addMultilayerIntraLink(
            layer_id, source_node_id, target_node_id, weight
        )

    def add_multilayer_intra_links(self, links):
        """Add several intra-layer links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> links = (
        ...     (1, 1, 2),
        ...     (1, 2, 3, 2.0),
        ...     (2, 1, 3),
        ... )
        >>> im.add_multilayer_intra_links(links)

        See Also
        --------
        add_multilayer_intra_link

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form
            ``(layer_id, source_node_id, target_node_id, [weight])``.
            NumPy arrays must be 2-dimensional with 3 or 4 columns.
        """
        if _is_numpy_input(links):
            links_array = _normalize_numpy_links(
                links,
                name="multilayer intra-link",
                valid_columns=(3, 4),
                column_description="(layer_id, source_node_id, target_node_id, [weight])",
            )
            return super().addMultilayerIntraLinksFromNumpy2D(
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

        return super().addMultilayerIntraLinks(
            layer_ids, source_node_ids, target_node_ids, weights
        )

    def add_multilayer_inter_link(
        self, source_layer_id, node_id, target_layer_id, weight=1.0
    ):
        """Add an inter-layer link.

        Adds a link between two layers in a multilayer network.
        The link is specified through a shared physical node, but
        that jump will not be recorded so Infomap will spread out
        this link to the next possible steps for the random walker
        in the target layer.


        Notes
        -----
        This multilayer format requires a directed network, so if
        the directed flag is not present, it will add all links
        also in their opposite direction to transform the undirected
        input to directed. If no inter-layer links are added, Infomap
        will simulate these by relaxing the random walker's constraint
        to its current layer. The final state network will be generated
        on run, which will clear the temporary data structure that holds
        the provided inter-layer links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> im.add_multilayer_inter_link(1, 1, 2)
        >>> im.add_multilayer_inter_link(1, 2, 2)
        >>> im.add_multilayer_inter_link(2, 1, 1)
        >>> im.add_multilayer_inter_link(2, 3, 1)

        Parameters
        ----------
        source_layer_id : int
        node_id : int
        target_layer_id : int
        weight : float, optional

        """
        return super().addMultilayerInterLink(
            source_layer_id, node_id, target_layer_id, weight
        )

    def add_multilayer_inter_links(self, links):
        """Add several inter-layer links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> links = (
        ...     (1, 1, 2),
        ...     (1, 2, 2, 2.0),
        ...     (2, 3, 1),
        ... )
        >>> im.add_multilayer_inter_links(links)

        See Also
        --------
        add_multilayer_inter_link

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form
            ``(source_layer_id, node_id, target_layer_id, [weight])``.
            NumPy arrays must be 2-dimensional with 3 or 4 columns.
        """
        if _is_numpy_input(links):
            links_array = _normalize_numpy_links(
                links,
                name="multilayer inter-link",
                valid_columns=(3, 4),
                column_description="(source_layer_id, node_id, target_layer_id, [weight])",
            )
            return super().addMultilayerInterLinksFromNumpy2D(
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

        return super().addMultilayerInterLinks(
            source_layer_ids, node_ids, target_layer_ids, weights
        )

    def add_multilayer_links(self, links):
        """Add several multilayer links.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap()
        >>> links = (
        ...     ((0, 1), (1, 2)),
        ...     ((0, 3), (1, 2))
        ... )
        >>> im.add_multilayer_links(links)


        See Also
        --------
        add_multilayer_link

        Parameters
        ----------
        links : iterable of tuples
            Iterable of tuples of the form
            ``(source_node, target_node, [weight])``. NumPy arrays must be
            2-dimensional with 4 or 5 columns of the form
            ``(source_layer_id, source_node_id, target_layer_id,
            target_node_id, [weight])``.
        """
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
            return super().addMultilayerLinksFromNumpy2D(
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

        return super().addMultilayerLinks(
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

    def set_meta_data(self, node_id, meta_category):
        """Set meta data to a node.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True, num_trials=10)
        >>> im.add_links((
        ...     (1, 2), (1, 3), (2, 3),
        ...     (3, 4),
        ...     (4, 5), (4, 6), (5, 6)
        ... ))
        >>> im.set_meta_data(1, 0)
        >>> im.set_meta_data(2, 0)
        >>> im.set_meta_data(3, 1)
        >>> im.set_meta_data(4, 1)
        >>> im.set_meta_data(5, 0)
        >>> im.set_meta_data(6, 0)
        >>> im.run(meta_data_rate=0)
        >>> im.num_top_modules
        2
        >>> im.run(meta_data_rate=2)
        >>> im.num_top_modules
        3


        Parameters
        ----------
        node_id : int
        meta_category : int
        """
        return self.network.addMetaData(node_id, meta_category)

    def add_networkx_graph(
        self,
        g,
        weight="weight",
        node_id="node_id",
        layer_id="layer_id",
        multilayer_inter_intra_format=True,
    ):
        """Add a NetworkX graph.

        Uses weighted links if present on the `weight` attribute.
        Treats the graph as a state network if the `node_id` attribute
        is present and as a multilayer network if also the `layer_id`
        attribute is present on the nodes.

        Examples
        --------

        >>> import networkx as nx
        >>> from infomap import Infomap
        >>> G = nx.Graph([("a", "b"), ("a", "c")])
        >>> im = Infomap(silent=True)
        >>> mapping = im.add_networkx_graph(G)
        >>> mapping
        {0: 'a', 1: 'b', 2: 'c'}
        >>> im.run()
        >>> for node in im.nodes:
        ...     print(node.node_id, node.module_id, node.flow, mapping[node.node_id])
        0 1 0.5 a
        1 1 0.25 b
        2 1 0.25 c

        Usage with a state network

        >>> import networkx as nx
        >>> from infomap import Infomap
        >>> G = nx.Graph()
        >>> G.add_node("a", node_id=1)
        >>> G.add_node("b", node_id=2)
        >>> G.add_node("c", node_id=3)
        >>> G.add_node("d", node_id=1)
        >>> G.add_node("e", node_id=4)
        >>> G.add_node("f", node_id=5)
        >>> G.add_edge("a", "b")
        >>> G.add_edge("a", "c")
        >>> G.add_edge("b", "c")
        >>> G.add_edge("d", "e")
        >>> G.add_edge("d", "f")
        >>> G.add_edge("e", "f")
        >>> im = Infomap(silent=True)
        >>> mapping = im.add_networkx_graph(G)
        >>> mapping
        {0: 'a', 1: 'b', 2: 'c', 3: 'd', 4: 'e', 5: 'f'}
        >>> im.run()
        >>> for node in im.nodes:
        ...     print(node.state_id, node.node_id, node.module_id, node.flow)
        0 1 1 0.16666666666666666
        1 2 1 0.16666666666666666
        2 3 1 0.16666666666666666
        3 1 2 0.16666666666666666
        4 4 2 0.16666666666666666
        5 5 2 0.16666666666666666

        Usage with a multilayer network

        >>> import networkx as nx
        >>> from infomap import Infomap
        >>> G = nx.Graph()
        >>> G.add_node(11, node_id=1, layer_id=1)
        >>> G.add_node(21, node_id=2, layer_id=1)
        >>> G.add_node(22, node_id=2, layer_id=2)
        >>> G.add_node(32, node_id=3, layer_id=2)
        >>> G.add_edge(11, 21, weight=2)
        >>> G.add_edge(22, 32)
        >>> im = Infomap(silent=True)
        >>> mapping = im.add_networkx_graph(G)
        >>> im.run()
        >>> for node in sorted(im.nodes, key=lambda n: n.state_id):
        ...     print(node.state_id, node.module_id, f"{node.flow:.2f}", node.node_id, node.layer_id)
        11 1 0.28 1 1
        21 1 0.28 2 1
        22 2 0.22 2 2
        32 2 0.22 3 2

        Notes
        -----
        Transforms non-int labels to unique int ids.
        Assumes that all nodes are of the same type.
        If node type is string, they are added as names
        to Infomap.
        If the NetworkX graph is directed (``nx.DiGraph``), and no flow
        model has been specified in the constructor, this method
        sets the ``directed`` flag to ``True``.

        Parameters
        ----------
        g : nx.Graph
            A NetworkX graph.
        weight : str, optional
            Key to look up link weight in edge data if present. Default
            ``"weight"``.
        node_id : str, optional
            Node attribute for physical node ids, implying a state network.
        layer_id : str, optional
            Node attribute for layer ids, implying a multilayer network.
        multilayer_inter_intra_format : bool, optional
            Use intra/inter format to simulate inter-layer links. Default
            ``True``.

        Returns
        -------
        dict
            Dict with the internal node ids as keys and original labels as
            values.
        """
        return _add_networkx_graph(
            self,
            g,
            weight=weight,
            node_id=node_id,
            layer_id=layer_id,
            multilayer_inter_intra_format=multilayer_inter_intra_format,
        )

    def add_scipy_sparse_matrix(self, A, directed=False, weighted=True, node_ids=None):
        """Add links and nodes from a SciPy sparse adjacency matrix.

        Parameters
        ----------
        A : scipy.sparse matrix or array
            Square sparse adjacency matrix.
        directed : bool, optional
            Interpret ``A[i, j]`` as a directed edge from row ``i`` to column
            ``j``. Default ``False``.
        weighted : bool, optional
            Use sparse matrix values as link weights. If ``False``, every
            nonzero entry is treated as weight ``1.0``. Default ``True``.
        node_ids : sequence, optional
            External node ids in matrix row order. If omitted, ``0..n-1`` is
            used.

        Returns
        -------
        dict
            Dict with internal integer node ids as keys and external node ids
            as values.
        """
        return _add_scipy_sparse_matrix(
            self,
            A,
            directed=directed,
            weighted=weighted,
            node_ids=node_ids,
        )

    def add_edge_index(
        self,
        edge_index,
        edge_weight=None,
        num_nodes=None,
        directed=True,
        node_ids=None,
    ):
        """Add links and nodes from a PyG-style edge index.

        Parameters
        ----------
        edge_index : array-like
            Two-row edge index where row 0 contains source node ids and row 1
            contains target node ids.
        edge_weight : array-like, optional
            One-dimensional edge weights with one value per edge. If omitted,
            every edge is treated as weight ``1.0``.
        num_nodes : int, optional
            Total number of nodes. Pass this to preserve isolated nodes.
        directed : bool, optional
            Interpret edges as directed. Default ``True``.
        node_ids : sequence, optional
            External node ids in internal node order. If omitted, ``0..n-1`` is
            used.

        Returns
        -------
        dict
            Dict with internal integer node ids as keys and external node ids
            as values.
        """
        return _add_edge_index(
            self,
            edge_index,
            edge_weight=edge_weight,
            num_nodes=num_nodes,
            directed=directed,
            node_ids=node_ids,
        )

    def add_igraph_graph(
        self,
        g,
        edge_weights=None,
        vertex_weights=None,
        node_id="node_id",
        layer_id="layer_id",
        multilayer_inter_intra_format=True,
    ):
        """Add a python-igraph graph.

        This method imports igraph lazily, so igraph is not required unless
        this method is used. It uses igraph's zero-based vertex indices as
        state/internal ids, uses the ``name`` vertex attribute as Infomap node
        names when present, and treats ``node_id``/``layer_id`` vertex
        attributes as state/multilayer metadata.

        Parameters
        ----------
        g : igraph.Graph
            A python-igraph graph.
        edge_weights : str, sequence, or None, optional
            Edge weight attribute name, explicit sequence with one value per
            edge, or ``None`` to treat every edge as weight 1. Default
            ``None``.
        vertex_weights : None, optional
            Accepted for igraph API familiarity but not supported yet.
        node_id : str, optional
            Vertex attribute for physical node ids, implying a state network.
        layer_id : str, optional
            Vertex attribute for layer ids, implying a multilayer network when
            ``node_id`` is also present.
        multilayer_inter_intra_format : bool, optional
            Use intra/inter format to simulate inter-layer links. Default
            ``True``.

        Returns
        -------
        dict
            Dict with igraph vertex indices as keys and vertex names as values
            when names are present, otherwise vertex indices as values.
        """
        return _add_igraph_graph(
            self,
            g,
            edge_weights=edge_weights,
            vertex_weights=vertex_weights,
            node_id=node_id,
            layer_id=layer_id,
            multilayer_inter_intra_format=multilayer_inter_intra_format,
        )

    # ----------------------------------------
    # Run
    # ----------------------------------------

    @property
    def bipartite_start_id(self):
        """Get or set the bipartite start id.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True, num_trials=10)
        >>> im.add_node(1, "Left 1")
        >>> im.add_node(2, "Left 2")
        >>> im.bipartite_start_id = 3
        >>> im.add_node(3, "Right 3")
        >>> im.add_node(4, "Right 4")
        >>> im.add_link(1, 3)
        >>> im.add_link(1, 4)
        >>> im.add_link(2, 4)
        >>> im.run()
        >>> tol = 1e-4
        >>> abs(im.codelength - 0.9182958340544896) < tol
        True


        Parameters
        ----------
        start_id : int
            The node id where the second node type starts.

        Returns
        -------
        int
            The node id where the second node type starts.
        """
        return self.network.bipartiteStartId()

    @bipartite_start_id.setter
    def bipartite_start_id(self, start_id):
        super().setBipartiteStartId(start_id)

    @property
    def initial_partition(self):
        """Get or set the initial partition.

        This is a initial configuration of nodes into modules where Infomap
        will start the optimizer.

        Examples
        --------

        >>> from infomap import Infomap
        >>> im = Infomap(silent=True)
        >>> im.add_node(1)
        >>> im.add_node(2)
        >>> im.add_node(3)
        >>> im.add_node(4)
        >>> im.add_link(1, 2)
        >>> im.add_link(1, 3)
        >>> im.add_link(2, 3)
        >>> im.add_link(2, 4)
        >>> im.initial_partition = {
        ...     1: 0,
        ...     2: 0,
        ...     3: 1,
        ...     4: 1
        ... }
        >>> im.run(no_infomap=True)
        >>> tol = 1e-4
        >>> abs(im.codelength - 3.4056390622295662) < tol
        True


        Notes
        -----
        The initial partition is saved between runs.
        If you want to use an initial partition for one run only,
        use ``run(initial_partition=partition)``.

        For a multilayer network you can key the partition by physical identity
        instead of state ids, using ``(layer_id, node_id)`` tuples (or
        :class:`MultilayerNode`) as keys. The resolution to internally generated
        state ids is deferred until the network is built when you call
        :meth:`run`.

        >>> from infomap import Infomap, MultilayerNode
        >>> im = Infomap(silent=True)
        >>> im.add_multilayer_intra_link(1, 1, 2)
        >>> im.add_multilayer_intra_link(2, 1, 3)
        >>> im.initial_partition = {(1, 1): 0, MultilayerNode(2, 1): 1}

        Parameters
        ----------
        module_ids : dict, or None
            Either ``{node_or_state_id: module_id}`` (integers) or, for a
            multilayer network, ``{(layer_id, node_id): module_id}``.

        Returns
        -------
        dict
            The initial partition as last set.
        """
        physical = getattr(self, "_physical_initial_partition", None)
        if physical is not None:
            return physical
        return super().getInitialPartition()

    @initial_partition.setter
    def initial_partition(self, module_ids):
        if module_ids is None:
            module_ids = {}
        keys = list(module_ids.keys())
        if keys and any(isinstance(key, tuple) for key in keys):
            if not all(isinstance(key, tuple) for key in keys):
                raise ValueError(
                    "initial_partition keys must be either all integers "
                    "(node/state ids) or all (layer_id, node_id) tuples, not mixed"
                )
            layer_ids, node_ids, modules = [], [], []
            for (layer_id, node_id), module in module_ids.items():
                layer_ids.append(int(layer_id))
                node_ids.append(int(node_id))
                modules.append(int(module))
            super().setMultilayerInitialPartition(layer_ids, node_ids, modules)
            self._physical_initial_partition = dict(module_ids)
        else:
            super().setInitialPartition(module_ids)
            self._physical_initial_partition = None

    @contextmanager
    def _initial_partition(self, partition):
        # Internal use only
        # Store the possibly set initial partition
        # and use the new initial partition for one run only.
        old_partition = self.initial_partition
        try:
            self.initial_partition = partition
            yield
        finally:
            self.initial_partition = old_partition

    def run(
        self,
        args=None,
        initial_partition=None,
        # input
        cluster_data=None,
        no_infomap=False,
        skip_adjust_bipartite_flow=False,
        bipartite_teleportation=False,
        weight_threshold=None,
        include_self_links=None,
        no_self_links=False,
        node_limit=None,
        matchable_multilayer_ids=None,
        assign_to_neighbouring_module=False,
        meta_data=None,
        meta_data_rate=1.0,
        meta_data_unweighted=False,
        # output
        tree=False,
        ftree=False,
        clu=False,
        verbosity_level=1,
        silent=False,
        pretty=False,
        out_name=None,
        no_file_output=False,
        clu_level=None,
        output=None,
        hide_bipartite_nodes=False,
        print_all_trials=False,
        no_overwrite=False,
        print_config_fingerprint=False,
        timing_json=None,
        summary_json=None,
        manifest_json=None,
        memory_report=False,
        # algorithm
        two_level=False,
        flow_model=None,
        directed=None,
        recorded_teleportation=False,
        use_node_weights_as_flow=False,
        to_nodes=False,
        teleportation_probability=0.15,
        regularized=False,
        regularization_strength=1.0,
        entropy_corrected=False,
        entropy_correction_strength=1.0,
        markov_time=1.0,
        variable_markov_time=False,
        variable_markov_damping=1.0,
        variable_markov_min_scale=1.0,
        preferred_number_of_modules=None,
        multilayer_relax_rate=0.15,
        multilayer_relax_limit=-1,
        multilayer_relax_limit_up=-1,
        multilayer_relax_limit_down=-1,
        multilayer_relax_by_jsd=False,
        # accuracy
        seed=123,
        num_trials=1,
        core_loop_limit=10,
        core_level_limit=None,
        tune_iteration_limit=None,
        core_loop_codelength_threshold=1e-10,
        tune_iteration_relative_threshold=1e-05,
        fast_hierarchical_solution=None,
        prefer_modular_solution=False,
        inner_parallelization=False,
        parallel_trials=False,
        num_threads=None,
        threads=None,
        trial_offset=None,
        trial_results=None,
        no_final_output=False,
        num_random_moves=None,
        max_degree_for_random_moves=None,
    ):
        """Run Infomap.

        Keyword arguments mirror the Infomap CLI flags. Use
        :class:`InfomapOptions` for the full parameter reference and
        :meth:`run_with_options` when reusing a saved configuration.

        Parameters
        ----------
        args : str, optional
            Raw Infomap arguments to prepend before rendered keyword options.
        initial_partition : dict, optional
            Initial partition to use for this run only. See
            :attr:`initial_partition`.

        See Also
        --------
        initial_partition
        """
        options = InfomapOptions.from_mapping(locals())
        args = _package_construct_args()(args, **options.to_kwargs())

        if initial_partition is not None:
            with self._initial_partition(initial_partition):
                return super().run(args)

        return super().run(args)

    def run_with_options(self, options, *, args=None, initial_partition=None):
        """Run Infomap using a reusable :class:`InfomapOptions` instance."""
        if not isinstance(options, InfomapOptions):
            raise TypeError("options must be an InfomapOptions instance")
        return self.run(
            args=args,
            initial_partition=initial_partition,
            **options.to_kwargs(),
        )

    @property
    def network(self):
        """Get the internal network."""
        return super().network()

    @property
    def codelength(self):
        """Get the total (hierarchical) codelength.

        See Also
        --------
        index_codelength
        module_codelength

        Returns
        -------
        float
            The codelength
        """
        return super().codelength()

    @property
    def codelengths(self):
        """Get the total (hierarchical) codelength for each trial.

        See Also
        --------
        codelength

        Returns
        -------
        tuple of float
            The codelengths for each trial
        """
        return super().codelengths()

    @property
    def elapsed_time(self):
        """Get the elapsed run time in seconds.

        Returns
        -------
        float
            The elapsed run time in seconds.
        """
        return super().elapsedTime()


def main():
    import sys

    args = " ".join(sys.argv[1:])
    return run(args)  # noqa: F405


if __name__ == "__main__":
    sys.exit(main())
