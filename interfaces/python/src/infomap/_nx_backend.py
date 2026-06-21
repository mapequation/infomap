"""NetworkX backend entry point for Infomap.

Registers ``infomap`` as a NetworkX backend so that
``nx.community.infomap_communities(G, backend="infomap")`` -- or via
``nx.config.backend_priority = ["infomap"]`` -- dispatches to the Infomap C++
engine. Implements ``infomap_communities`` (two-level) and ``infomap_partitions``
(multilevel) through the low-level :class:`infomap.Infomap` API.

Any extra Infomap option may be passed as a backend-only keyword argument and
is forwarded to :class:`infomap.Infomap`, e.g.::

    nx.community.infomap_communities(
        G, backend="infomap", markov_time=1.5, flow_model="directed",
    )
"""

from __future__ import annotations

from ._facade import Infomap


def _build_infomap(G, weight, options):
    """Construct a low-level :class:`Infomap` for `G`, without running it.

    Returns ``(im, mapping)`` where ``mapping`` is ``{infomap_node_id: label}``
    for recovering the original node labels. Graph-ingestion options are routed
    to :meth:`Infomap.add_networkx_graph`; everything else configures Infomap.
    """
    # Multigraphs are accepted: add_networkx_graph sums parallel edges, matching
    # the native infomap_communities (and Louvain).
    options = dict(options)
    options.setdefault("silent", True)
    options.setdefault("no_file_output", True)
    # Without this a DiGraph would be read with the undirected flow model.
    options.setdefault("directed", G.is_directed())
    # node_id/layer_id name the node attributes add_networkx_graph reads to
    # detect state/multilayer networks; sentinel names keep a plain graph from
    # being treated as one (advanced users override via backend kwargs).
    add_graph_kwargs = {"weight": weight}
    for key, default in (
        ("node_id", "__infomap_nx_no_state__"),
        ("layer_id", "__infomap_nx_no_layer__"),
    ):
        add_graph_kwargs[key] = options.pop(key, default)
    if "multilayer_inter_intra_format" in options:
        add_graph_kwargs["multilayer_inter_intra_format"] = options.pop(
            "multilayer_inter_intra_format"
        )
    im = Infomap(**options)
    mapping = im.add_networkx_graph(G, **add_graph_kwargs)
    return im, mapping


def _run_infomap(G, weight, seed, num_trials, infomap_kwargs, *, two_level):
    """Build, run, and return ``(im, mapping)`` for the optimizing functions."""
    options = dict(infomap_kwargs)
    if seed is not None:
        # NetworkX passes a random-state object (py_random_state); Infomap's C++
        # RNG needs an integer seed, so draw one from it.
        if hasattr(seed, "randint"):
            seed = seed.randint(0, 2**31 - 1)
        options.setdefault("seed", int(seed))
    options.setdefault("num_trials", num_trials)
    if two_level:
        options.setdefault("two_level", True)
    im, mapping = _build_infomap(G, weight, options)
    im.run()
    return im, mapping


def _grouped(node_modules, mapping):
    """Group infomap node ids by their module key into a list of label sets,
    restoring the original NetworkX node labels via ``mapping``."""
    groups: dict = {}
    for node_id, module in node_modules.items():
        groups.setdefault(module, set()).add(mapping[node_id])
    return list(groups.values())


class BackendInterface:
    @staticmethod
    def infomap_communities(
        G, weight="weight", seed=None, num_trials=1, **infomap_kwargs
    ):
        im, mapping = _run_infomap(
            G, weight, seed, num_trials, infomap_kwargs, two_level=True
        )
        return _grouped(im.get_modules(), mapping)

    @staticmethod
    def infomap_partitions(
        G, weight="weight", seed=None, num_trials=1, **infomap_kwargs
    ):
        im, mapping = _run_infomap(
            G, weight, seed, num_trials, infomap_kwargs, two_level=False
        )
        # get_multilevel_modules() maps each node to its tuple of module ids, top
        # level first. Emit one flat partition per level, coarsest first.
        multilevel = im.get_multilevel_modules()
        depth = max((len(path) for path in multilevel.values()), default=0)
        return [
            _grouped({node: path[:level] for node, path in multilevel.items()}, mapping)
            for level in range(1, depth + 1)
        ]

    @staticmethod
    def map_equation(
        G, communities, weight="weight", teleportation_prob=0.15, **infomap_kwargs
    ):
        options = dict(infomap_kwargs)
        options.setdefault("two_level", True)
        options.setdefault("teleportation_probability", teleportation_prob)
        im, mapping = _build_infomap(G, weight, options)
        label_to_id = {label: node_id for node_id, label in mapping.items()}
        partition = {
            label_to_id[label]: module
            for module, community in enumerate(communities)
            for label in community
        }
        # no_infomap evaluates the given partition's codelength without
        # optimizing; with two_level it is the two-level map equation.
        im.run(initial_partition=partition, no_infomap=True)
        return im.codelength

    # Infomap consumes NetworkX graphs directly (it has no native graph type),
    # so conversion in both directions is the identity.
    @staticmethod
    def convert_from_nx(graph, **kwargs):
        return graph

    @staticmethod
    def convert_to_nx(obj, **kwargs):
        return obj


def get_info():
    return {
        "backend_name": "infomap",
        "project": "infomap",
        "package": "infomap",
        "url": "https://www.mapequation.org/infomap",
        "short_summary": "Flow-based community detection with the map equation.",
        "functions": {
            "infomap_communities": {
                "url": "https://www.mapequation.org/infomap",
                "additional_docs": (
                    "Minimizes the map equation. Extra Infomap options may be "
                    "passed as backend-only keyword arguments."
                ),
                "additional_parameters": {
                    "**infomap_kwargs : Any": (
                        "Options forwarded to infomap.Infomap, e.g. two_level, "
                        "flow_model, markov_time, regularized, variable_markov_time."
                    ),
                },
            },
            "infomap_partitions": {
                "url": "https://www.mapequation.org/infomap",
                "additional_docs": (
                    "Yields the multilevel map equation partition at each "
                    "hierarchy level, coarsest first. Extra Infomap options may "
                    "be passed as backend-only keyword arguments."
                ),
                "additional_parameters": {
                    "**infomap_kwargs : Any": (
                        "Options forwarded to infomap.Infomap, e.g. flow_model, "
                        "markov_time, regularized, variable_markov_time."
                    ),
                },
            },
            "map_equation": {
                "url": "https://www.mapequation.org/infomap",
                "additional_docs": (
                    "Two-level map equation codelength of the given partition, "
                    "evaluated by the Infomap engine without optimizing."
                ),
                "additional_parameters": {
                    "**infomap_kwargs : Any": (
                        "Options forwarded to infomap.Infomap, e.g. flow_model, "
                        "markov_time, regularized, variable_markov_time."
                    ),
                },
            },
        },
    }
