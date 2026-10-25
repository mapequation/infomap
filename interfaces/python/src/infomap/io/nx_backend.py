"""NetworkX backend entry point for Infomap.

Registers ``infomap`` as a NetworkX backend so that
``nx.community.infomap_communities(G, backend="infomap")`` -- or via
``nx.config.backend_priority = ["infomap"]`` -- dispatches to the Infomap C++
engine. Implements the three NetworkX map equation functions:
``infomap_communities`` (two-level), ``infomap_partitions`` (multilevel) and
``map_equation`` (codelength of a given partition). NetworkX defines these
functions from 3.8 on; with an older NetworkX the backend is registered but
has nothing to implement, and NetworkX works as before.

Each function takes NetworkX's own arguments verbatim -- ``weight``, ``seed``,
``num_trials``, ``teleportation_probability`` -- because NetworkX names them
after this package's options. Any further Infomap option may be passed as a
backend-only keyword argument, forwarded to the run, e.g.::

    nx.community.infomap_communities(
        G, backend="infomap", markov_time=1.5, flow_model="directed",
    )
"""

from __future__ import annotations

from math import isfinite

# Safe at module top: unlike the io/ graph adapters (imported by the facade,
# hence lazy to avoid a cycle), this backend module is only imported via the
# networkx entry point, after the facade is fully loaded. networkx itself stays
# a lazy import: networkx calls get_info() while it is still importing.
from ..errors import InfomapError
from ..network import Network

# node_id_attribute/layer_id_attribute name the node attributes the networkx
# adapter reads to detect state/multilayer networks; sentinel names keep a plain
# graph from being treated as one (advanced users override via backend kwargs).
_NO_STATE = "__infomap_nx_no_state__"
_NO_LAYER = "__infomap_nx_no_layer__"

# Backend kwargs that configure Network.from_networkx rather than the run.
_INGEST_OPTIONS = ("multilayer_inter_intra_format", "meta_attribute")


def _network(G, weight, options):
    """Build a :class:`Network` from `G`, without running it.

    Returns ``(net, engine_options)``: graph-ingestion options are consumed
    here, the rest configure the run.
    """
    # Multigraphs are accepted: the adapter sums parallel edges, matching the
    # native infomap_communities (and Louvain).
    options = dict(options)
    build_kwargs = {"weight": weight}
    for key, old_key, default in (
        ("node_id_attribute", "node_id", _NO_STATE),
        ("layer_id_attribute", "layer_id", _NO_LAYER),
    ):
        if old_key in options and key not in options:
            # The pre-2.16 spelling: pass it on, so the caller gets the
            # adapter's deprecation warning rather than a silent rename.
            build_kwargs[old_key] = options.pop(old_key)
        else:
            build_kwargs[key] = options.pop(key, default)
    for key in _INGEST_OPTIONS:
        if key in options:
            build_kwargs[key] = options.pop(key)
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


def _check_num_trials(num_trials):
    # Match the native contract (ValueError, not the engine's RuntimeError) so
    # the two code paths agree.
    if not isinstance(num_trials, int) or num_trials < 1:
        raise ValueError("num_trials must be a positive integer")


def _teleportation(G, teleportation_probability, options):
    """Return the engine options that carry `teleportation_probability`.

    Only a directed flow model reads it. Like the native functions, an
    undirected graph accepts any value, and it never reaches the engine (which
    would reject a nan); a caller who names a flow model gets it checked and
    passed on.
    """
    if not G.is_directed() and "flow_model" not in options:
        return {}
    if not 0 <= teleportation_probability <= 1:
        raise ValueError("teleportation_probability must be between 0 and 1")
    return {"teleportation_probability": teleportation_probability}


def _run_network(G, net, weight, options):
    """Run `net`, reporting a weight total that overflows as the native
    functions do."""
    try:
        return net.run(**options)
    except InfomapError as exc:
        # The engine checks each weight at ingestion, but a total that overflows
        # only shows up later, as degenerate flow. Look for it here, after the
        # run has failed, so a successful run pays nothing for the check.
        if not isfinite(2 * G.size(weight=weight)):
            raise ValueError("edge weights are too large to sum as floats") from exc
        raise


def _run(G, weight, seed, num_trials, teleportation, infomap_kwargs, *, two_level):
    """Build, run, and return ``(result, id_to_label)`` for the optimizers.

    The arguments are already checked: `teleportation` is what
    :func:`_teleportation` returned.
    """
    net, options = _network(G, weight, infomap_kwargs)
    if seed is not None:
        # NetworkX passes a random-state object (py_random_state); Infomap's C++
        # RNG needs an integer seed, so draw one from it.
        if hasattr(seed, "randint"):
            seed = seed.randint(0, 2**31 - 1)
        options.setdefault("seed", int(seed))
    options.setdefault("num_trials", num_trials)
    options.update(teleportation)
    if two_level:
        options.setdefault("two_level", True)
    # Read the modules off the Result the run returns, not off the engine: the
    # instance result accessors are deprecated and leave in 3.0.
    return _run_network(G, net, weight, options), net.node_id_to_label


def _grouped(node_modules, id_to_label):
    """Group node ids by their module key into a list of label sets, restoring
    the original NetworkX node labels."""
    groups: dict = {}
    for node_id, module in node_modules.items():
        groups.setdefault(module, set()).add(id_to_label[node_id])
    return list(groups.values())


def _levels(multilevel, id_to_label):
    """Yield one flat partition per hierarchy level, coarsest first.

    `multilevel` maps each node to its tuple of module ids, top level first. A
    node that bottoms out above the deepest level keeps its own leaf module,
    which slicing its shorter path preserves.
    """
    depth = max((len(path) for path in multilevel.values()), default=0)
    for level in range(1, depth + 1):
        yield _grouped(
            {node: path[:level] for node, path in multilevel.items()}, id_to_label
        )


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
        import networkx as nx

        _check_num_trials(num_trials)
        teleportation = _teleportation(G, teleportation_probability, infomap_kwargs)
        if nx.is_empty(G):  # no edges, so no flow to compress: every node is alone
            return [{u} for u in G]
        result, id_to_label = _run(
            G,
            weight,
            seed,
            num_trials,
            teleportation,
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
        import networkx as nx

        # Not a generator function: like the native infomap_partitions, check
        # the arguments and run the search on the call, so a bad argument raises
        # there, and hand out only the levels lazily.
        _check_num_trials(num_trials)
        teleportation = _teleportation(G, teleportation_probability, infomap_kwargs)
        if nx.is_empty(G):  # one level of singletons, as the native function gives
            return iter([[{u} for u in G]])
        result, id_to_label = _run(
            G,
            weight,
            seed,
            num_trials,
            teleportation,
            infomap_kwargs,
            two_level=False,
        )
        return _levels(result.multilevel_modules(), id_to_label)

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
        from networkx.algorithms.community.quality import NotAPartition

        # Same guard, same error as the native map_equation: a codelength of
        # something that is not a partition of G is meaningless. Check the
        # communities as given, so a node listed twice in one is caught too.
        if not isinstance(communities, list):
            communities = list(communities)
        if not nx.community.is_partition(G, communities):
            raise NotAPartition(G, communities)
        options = dict(infomap_kwargs)
        options.update(_teleportation(G, teleportation_probability, options))
        if nx.is_empty(G):  # no edges, so no flow and nothing to code
            return 0.0
        options.setdefault("two_level", True)
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
        options["initial_partition"] = partition
        return _run_network(G, net, weight, options).codelength

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
