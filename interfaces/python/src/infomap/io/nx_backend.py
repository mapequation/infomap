"""NetworkX backend entry point for Infomap.

Registers ``infomap`` as a NetworkX backend so that
``nx.community.infomap_communities(G, backend="infomap")`` -- or via
``nx.config.backend_priority = ["infomap"]`` -- dispatches to the Infomap C++
engine. Implements the three NetworkX map equation functions:
``infomap_communities`` (two-level), ``infomap_partitions`` (multilevel) and
``map_equation`` (codelength of a given partition).

Each function takes NetworkX's own arguments verbatim -- ``weight``, ``seed``,
``num_trials``, ``teleportation_probability`` -- because NetworkX names them
after this package's options. Any further Infomap option may be passed as a
backend-only keyword argument, forwarded to the run, e.g.::

    nx.community.infomap_communities(
        G, backend="infomap", markov_time=1.5, flow_model="directed",
    )
"""

from __future__ import annotations

# Safe at module top: unlike the io/ graph adapters (imported by the facade,
# hence lazy to avoid a cycle), this backend module is only imported via the
# networkx entry point, after the facade is fully loaded.
from ..network import Network

# node_id/layer_id name the node attributes the networkx adapter reads to detect
# state/multilayer networks; sentinel names keep a plain graph from being treated
# as one (advanced users override via backend kwargs).
_NO_STATE = "__infomap_nx_no_state__"
_NO_LAYER = "__infomap_nx_no_layer__"


def _network(G, weight, options):
    """Build a :class:`Network` from `G`, without running it.

    Returns ``(net, engine_options)``: graph-ingestion options are consumed
    here, the rest configure the run.
    """
    # Multigraphs are accepted: the adapter sums parallel edges, matching the
    # native infomap_communities (and Louvain).
    options = dict(options)
    build_kwargs = {"weight": weight}
    for key, default in (("node_id", _NO_STATE), ("layer_id", _NO_LAYER)):
        build_kwargs[key] = options.pop(key, default)
    if "multilayer_inter_intra_format" in options:
        build_kwargs["multilayer_inter_intra_format"] = options.pop(
            "multilayer_inter_intra_format"
        )
    try:
        net = Network.from_networkx(G, **build_kwargs)
    except RuntimeError as exc:
        # The engine validates link weights at ingestion (cheaper than a
        # per-edge Python pass) but reports them as InfomapError, a
        # RuntimeError. Translate that one case to ValueError so the backend
        # matches the native functions' contract.
        if "weight" in str(exc).lower():
            raise ValueError(str(exc)) from exc
        raise
    # Without this a DiGraph would be read with the undirected flow model. A
    # caller who names a flow model has said which one they want, so leave it
    # alone -- `directed` is shorthand for one of them and would override it.
    if G.is_directed() and "flow_model" not in options:
        options.setdefault("directed", True)
    return net, options


def _run(
    G,
    weight,
    seed,
    num_trials,
    teleportation_probability,
    infomap_kwargs,
    *,
    two_level,
):
    """Build, run, and return ``(result, id_to_label)`` for the optimizers."""
    # Match the native contract (ValueError, not the engine's RuntimeError) so
    # the two code paths agree.
    if not isinstance(num_trials, int) or num_trials < 1:
        raise ValueError("num_trials must be a positive integer")
    net, options = _network(G, weight, infomap_kwargs)
    if seed is not None:
        # NetworkX passes a random-state object (py_random_state); Infomap's C++
        # RNG needs an integer seed, so draw one from it.
        if hasattr(seed, "randint"):
            seed = seed.randint(0, 2**31 - 1)
        options.setdefault("seed", int(seed))
    options.setdefault("num_trials", num_trials)
    options.setdefault("teleportation_probability", teleportation_probability)
    if two_level:
        options.setdefault("two_level", True)
    # Read the modules off the Result the run returns, not off the engine: the
    # instance result accessors are deprecated and leave in 3.0.
    return net.run(**options), net.node_id_to_label


def _grouped(node_modules, id_to_label):
    """Group node ids by their module key into a list of label sets, restoring
    the original NetworkX node labels."""
    groups: dict = {}
    for node_id, module in node_modules.items():
        groups.setdefault(module, set()).add(id_to_label[node_id])
    return list(groups.values())


class BackendInterface:
    @staticmethod
    def infomap_communities(
        G,
        *,
        weight="weight",
        seed=None,
        num_trials=1,
        teleportation_probability=0.15,
        **infomap_kwargs,
    ):
        result, id_to_label = _run(
            G,
            weight,
            seed,
            num_trials,
            teleportation_probability,
            infomap_kwargs,
            two_level=True,
        )
        return _grouped(result.modules(), id_to_label)

    @staticmethod
    def infomap_partitions(
        G,
        *,
        weight="weight",
        seed=None,
        num_trials=1,
        teleportation_probability=0.15,
        **infomap_kwargs,
    ):
        result, id_to_label = _run(
            G,
            weight,
            seed,
            num_trials,
            teleportation_probability,
            infomap_kwargs,
            two_level=False,
        )
        # multilevel_modules() maps each node to its tuple of module ids, top
        # level first. Yield one flat partition per level, coarsest first -- a
        # generator, like the native infomap_partitions; a node that bottoms out
        # above the deepest level keeps its own leaf module, which slicing its
        # shorter path preserves.
        multilevel = result.multilevel_modules()
        depth = max((len(path) for path in multilevel.values()), default=0)
        for level in range(1, depth + 1):
            yield _grouped(
                {node: path[:level] for node, path in multilevel.items()}, id_to_label
            )

    @staticmethod
    def map_equation(
        G,
        communities,
        *,
        weight="weight",
        teleportation_probability=0.15,
        **infomap_kwargs,
    ):
        import networkx as nx

        communities = [set(c) for c in communities]
        # Same guard, same error type as the native map_equation: a codelength
        # of something that is not a partition of G is meaningless.
        if not nx.community.is_partition(G, communities):
            raise nx.NetworkXError(
                "`communities` is not a partition of the nodes of `G`"
            )
        options = dict(infomap_kwargs)
        options.setdefault("two_level", True)
        options.setdefault("teleportation_probability", teleportation_probability)
        # no_infomap evaluates the given partition's codelength without
        # optimizing; with two_level it is the two-level map equation.
        options.setdefault("no_infomap", True)
        net, options = _network(G, weight, options)
        label_to_id = {
            label: node_id for node_id, label in net.node_id_to_label.items()
        }
        partition = {
            label_to_id[label]: module
            for module, community in enumerate(communities)
            for label in community
        }
        result = net.run(initial_partition=partition, **options)
        return result.codelength

    # Infomap consumes NetworkX graphs directly (it has no native graph type),
    # so conversion in both directions is the identity.
    @staticmethod
    def convert_from_nx(graph, **kwargs):
        return graph

    @staticmethod
    def convert_to_nx(obj, **kwargs):
        return obj


_EXTRA_OPTIONS = (
    "Options forwarded to the Infomap run, e.g. flow_model, markov_time, "
    "regularized, variable_markov_time. ``inspect.getdoc(infomap.Options)`` is "
    "the full reference."
)


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
                    "**infomap_kwargs : Any": _EXTRA_OPTIONS,
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
                    "**infomap_kwargs : Any": _EXTRA_OPTIONS,
                },
            },
            "map_equation": {
                "url": "https://www.mapequation.org/infomap",
                "additional_docs": (
                    "Two-level map equation codelength of the given partition, "
                    "evaluated by the Infomap engine without optimizing."
                ),
                "additional_parameters": {
                    "**infomap_kwargs : Any": _EXTRA_OPTIONS,
                },
            },
        },
    }
