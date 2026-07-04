import sys
from collections import namedtuple
from contextlib import contextmanager

from ._core import Core
from ._core import apply_initial_partition
from ._core import build_info as _engine_build_info
from ._core import run as _cli_run

# Documented tree-walking iterator/node types returned by ``Infomap.tree`` /
# ``leaf_modules`` / the physical variants (source/api/iterators.rst). Surfaced
# at the package top level so ``from infomap import InfoNode`` and
# ``isinstance(node, infomap.InfomapIterator)`` keep working. Reached through the
# ``_core`` boundary, never ``_swig``/``_bindings`` directly.
from ._core import (
    InfoNode,
    InfomapIterator,
    InfomapIteratorPhysical,
    InfomapLeafIterator,
    InfomapLeafIteratorPhysical,
    InfomapLeafModuleIterator,
)
from ._network_input import add_bulk_links as _add_bulk_links
from ._network_input import first_order_unpacker as _first_order_unpacker
from ._network_input import flat_multilayer_unpacker as _flat_multilayer_unpacker
from ._network_input import paired_multilayer_unpacker as _paired_multilayer_unpacker
from ._options import (
    InfomapOptions,
    Options,
    _construct_args,
)
from ._results import _InfomapResultsMixin
from ._results import entropy, perplexity, plogp
from .result import Result, TreeNode, build_result
from ._summary import (
    repr_html as _repr_html,
    repr_text as _repr_text,
    summary_data as _summary_data,
)
from .io.edge_index import add_edge_index as _add_edge_index
from .io.igraph import add_igraph_graph as _add_igraph_graph
from .io.igraph import find_igraph_communities
from .io.networkx import add_networkx_graph as _add_networkx_graph
from .io.networkx import find_communities
from .io.export import to_igraph, to_networkx
from .io.scipy import add_scipy_sparse_matrix as _add_scipy_sparse_matrix
from .io.writers import _InfomapWritersMixin
from .network import Network
from ._run import run


def _package_construct_args():
    if __package__ is None:
        return _construct_args
    package_module = sys.modules.get(__package__)
    if package_module is None:
        return _construct_args
    return getattr(package_module, "_construct_args", _construct_args)


MultilayerNode = namedtuple("MultilayerNode", "layer_id, node_id")


__all__ = [
    "InfoNode",
    "Infomap",
    "InfomapIterator",
    "InfomapIteratorPhysical",
    "InfomapLeafIterator",
    "InfomapLeafIteratorPhysical",
    "InfomapLeafModuleIterator",
    "InfomapOptions",
    "MultilayerNode",
    "Network",
    "Options",
    "Result",
    "TreeNode",
    "build_info",
    "entropy",
    "find_communities",
    "find_igraph_communities",
    "main",
    "perplexity",
    "plogp",
    "run",
    "to_igraph",
    "to_networkx",
]


class Infomap(_InfomapResultsMixin, _InfomapWritersMixin):
    """The stateful entry point to the algorithm: build a network with the
    ``add_*`` verbs, then call :meth:`run` to get an immutable
    :class:`~infomap.Result`. Internally it composes a :class:`~infomap.Network`
    (input) and an :class:`~infomap.Options` config over a single ``Core``
    boundary to the SWIG-compiled engine, rather than exposing that engine
    directly. For one-shot use prefer the functional :func:`infomap.run`; for
    incremental construction prefer :class:`~infomap.Network`.

    Examples
    --------
    Build a network, run Infomap, and read the results off the returned
    :class:`~infomap.Result`:

    >>> from infomap import Infomap
    >>> im = Infomap(silent=True)
    >>> im.add_node(1)
    >>> im.add_node(2)
    >>> im.add_link(1, 2)
    >>> result = im.run()
    >>> result.codelength
    1.0


    Read a network file and inspect a few metrics on the result:

    >>> from infomap import Infomap
    >>> im = Infomap(silent=True, num_trials=10)
    >>> im.read_file("ninetriangles.net")
    >>> result = im.run()
    >>> result.codelength
    3.3858
    >>> result.num_top_modules
    3


    Iterate the partition via :meth:`Result.modules` (``node_id -> module_id``)
    or :meth:`Result.nodes` (per-node views):

    >>> from infomap import Infomap
    >>> im = Infomap(silent=True)
    >>> im.add_links(((1, 2), (1, 3), (2, 3), (4, 5), (4, 6), (5, 6), (3, 4)))
    >>> result = im.run()
    >>> for node_id, module_id in sorted(result.modules().items()):
    ...     print(node_id, module_id)
    1 1
    2 1
    3 1
    4 2
    5 2
    6 2


    ``run()`` returns an immutable :class:`~infomap.Result`; read collections
    via methods (``result.modules()``, ``result.nodes()``, ``result.tree()``,
    ``result.links()``, ``result.to_dataframe()``) and scalars via properties
    (``result.codelength``, ``result.num_top_modules``).

    For more examples, see the examples directory.
    """

    def _init_from_options(self, args, options):
        self._core = Core(_package_construct_args()(args, **options.to_kwargs()))
        self._network = Network(core=self._core)
        self.node_id_to_label = {}
        # Run-generation token: incremented on every run(). A Result stamps the
        # generation it was created in; node-level access on a stale Result
        # (engine re-ran since) raises instead of reading freed memory. The C++
        # result tree is rebuilt on every run() (design §7).
        self._generation = 0
        self._result = None

    # === BEGIN generated: Infomap option signatures (scripts/generate_binding_options.py) ===
    def __init__(
        self,
        args=None,
        include_self_links=None,
        skip_adjust_bipartite_flow=False,
        bipartite_teleportation=False,
        weight_threshold=None,
        no_self_links=False,
        node_limit=None,
        matchable_multilayer_ids=None,
        cluster_data=None,
        assign_to_neighbouring_module=False,
        meta_data=None,
        meta_data_rate=1.0,
        meta_data_unweighted=False,
        no_infomap=False,
        out_name=None,
        no_file_output=False,
        tree=False,
        ftree=False,
        clu=False,
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
        trial_offset=None,
        trial_results=None,
        no_final_output=False,
        verbosity_level=1,
        silent=False,
        pretty=False,
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
        preferred_number_of_levels=None,
        preferred_number_of_levels_strength=1.0,
        multilayer_relax_rate=0.15,
        multilayer_relax_limit=-1,
        multilayer_relax_limit_up=-1,
        multilayer_relax_limit_down=-1,
        multilayer_relax_by_jsd=False,
        multilayer_relax_to_self=False,
        seed=123,
        num_trials=1,
        core_loop_limit=10,
        core_level_limit=None,
        tune_iteration_limit=None,
        core_loop_codelength_threshold=1e-10,
        tune_iteration_relative_threshold=1e-05,
        fast_hierarchical_solution=None,
        inner_parallelization=False,
        parallel_trials=False,
        converge=False,
        num_threads=None,
        threads=None,
        prefer_modular_solution=False,
        num_random_moves=None,
        max_degree_for_random_moves=None,
    ):
        """Create a new Infomap instance.

        Keyword arguments mirror the Infomap CLI flags. Use
        :class:`Options` for a reusable
        configuration object and the full parameter reference.

        Parameters
        ----------
        args : str, optional
            Raw Infomap arguments to prepend before rendered keyword options.
        include_self_links : bool, optional
            Deprecated. Self-links are included by default; use no_self_links=True to
            exclude them.
        skip_adjust_bipartite_flow : bool, optional
            Keep flow on bipartite nodes instead of distributing it to primary nodes.
        bipartite_teleportation : bool, optional
            Use bipartite teleportation instead of the default two-step unipartite
            teleportation.
        weight_threshold : float, optional
            Ignore input links with weight below this threshold.
        no_self_links : bool, optional
            Exclude self-links from the input network.
        node_limit : int, optional
            Read only nodes up to this node id and ignore links connected to higher node
            ids.
        matchable_multilayer_ids : int, optional
            Construct state ids from node ids and layer ids that stay comparable across
            networks. Set at least to the largest layer id among networks to match.
        cluster_data : str, optional
            Read an initial partition from a clu file or a hierarchy from a tree/ftree
            file. Tree input may use physical or state nodes for higher-order networks.
        assign_to_neighbouring_module : bool, optional
            With --cluster-data, assign nodes missing module ids to a neighboring node's
            module when possible.
        meta_data : str, optional
            Read metadata to encode from a clu-format file.
        meta_data_rate : float, optional
            With --meta-data, set the metadata encoding rate. The default encodes
            metadata at each step.
        meta_data_unweighted : bool, optional
            With --meta-data, encode metadata without weighting by node flow.
        no_infomap : bool, optional
            Skip optimization. Use this to calculate codelength for --cluster-data or to
            print non-modular statistics.
        out_name : str, optional
            Base name for output files, for example [out_directory]/[out-name].tree.
        no_file_output : bool, optional
            Do not write output files.
        tree : bool, optional
            Write the modular hierarchy to a tree file. Enabled by default when no other
            output format is selected.
        ftree : bool, optional
            Write the modular hierarchy and aggregated links between nested modules to
            an ftree file. Used by Network Navigator.
        clu : bool, optional
            Write top-level module ids for each node to a clu file.
        clu_level : int, optional
            With --clu or --output clu, write module ids at this depth from the root.
            Use -1 for bottom-level modules.
        output : sequence of str, optional
            Write selected output formats as a comma-separated list without spaces, e.g.
            -o clu,tree,ftree. Options: clu, tree, ftree, newick, json, csv, network,
            states, flow.
        hide_bipartite_nodes : bool, optional
            Hide bipartite nodes in output by projecting the solution to primary nodes.
        print_all_trials : bool, optional
            Write each trial to separate output files. Has effect only when --num-trials
            is greater than 1.
        no_overwrite : bool, optional
            Fail with an output error if any target output file already exists. By
            default existing files are replaced.
        print_config_fingerprint : bool, optional
            Print the canonical configuration fingerprint and exit.
        timing_json : str, optional
            Write machine-readable run timing JSON to this path. Use - for stdout.
        summary_json : str, optional
            Write machine-readable final run summary JSON to this path. Use - for
            stdout.
        manifest_json : str, optional
            Write a machine-readable run manifest JSON to this path. Use - for stdout.
        memory_report : bool, optional
            Include peak RSS and best-effort bytes per node/link estimates in timing
            JSON. Requires --timing-json.
        trial_offset : int, optional
            Global index of the first trial this process runs; trial i uses seed =
            base_seed + (trial_offset + i). Default 0 (single-process behavior).
        trial_results : str, optional
            Write this shard's per-trial results (codelengths, seeds, best-tree
            reference, fingerprints) as JSON to this path, for deterministic merging of
            distributed shard runs into a final solution.
        no_final_output : bool, optional
            Skip writing this process's aggregate best result. Per-trial outputs and
            --trial-results are still written.
        verbosity_level : int, optional
            Verbosity level on the console. 1 keeps the default output level, 2 renders
            -vv and so on.
        silent : bool, optional
            Suppress console output.
        pretty : bool, optional
            Deprecated. Accepted for backward compatibility; has no effect.
        two_level : bool, optional
            Optimize a two-level partition instead of the default multi-level hierarchy.
        flow_model : str, optional
            Choose how Infomap derives flow from the input links. Options: undirected,
            directed, undirdir, outdirdir, rawdir, precomputed.
        directed : bool, optional
            Treat input links as directed. Shorthand for --flow-model directed.
        recorded_teleportation : bool, optional
            When teleportation is used to calculate flow, also record teleportation
            steps in the codelength.
        use_node_weights_as_flow : bool, optional
            Use node weights from the API or Pajek node records as normalized node flow.
        to_nodes : bool, optional
            Teleport to nodes instead of links. Uses uniform node weights unless node
            weights are provided.
        teleportation_probability : float, optional
            Set the probability of teleporting to a random node or link when calculating
            flow.
        regularized : bool, optional
            Add a fully connected Bayesian prior network to reduce overfitting to
            missing links. Activates --recorded-teleportation.
        regularization_strength : float, optional
            Scale the relative strength of the Bayesian prior network used by
            --regularized.
        entropy_corrected : bool, optional
            Correct for negative entropy bias in small samples, especially solutions
            with many modules.
        entropy_correction_strength : float, optional
            Scale the default correction used by --entropy-corrected.
        markov_time : float, optional
            Scale link flow to change the cost of moving between modules. Higher values
            result in fewer modules.
        variable_markov_time : bool, optional
            Vary Markov time locally to reduce overpartitioning in sparse areas while
            keeping higher resolution in dense areas.
        variable_markov_damping : float, optional
            With --variable-markov-time, set damping between local effective degree (0)
            and local entropy (1).
        variable_markov_min_scale : float, optional
            With --variable-markov-time, set the minimum local scale for zero-entropy
            nodes. Local Markov time is max scale divided by local scale.
        preferred_number_of_modules : int, optional
            Penalize solutions by how far their number of modules differs from this
            value.
        preferred_number_of_levels : int, optional
            Soft preference for the depth of the hierarchy. Steering to a shallower
            depth is reliable at a small codelength cost; deeper is best-effort, bounded
            by what the optimizer proposes. No-op with --two-level or strength 0.
        preferred_number_of_levels_strength : float, optional
            Scale the strength of --preferred-number-of-levels. 0 disables the
            preference; larger values increase the cost of deviating from the preferred
            depth.
        multilayer_relax_rate : float, optional
            Set the probability of relaxing from a state node to neighboring layers
            instead of staying in the current layer.
        multilayer_relax_limit : int, optional
            Limit relaxation to this many neighboring layer ids in each direction. Use a
            negative value to allow relaxation to any layer.
        multilayer_relax_limit_up : int, optional
            Limit relaxation upward to this many higher neighboring layer ids. Use a
            negative value to allow relaxation to any higher layer.
        multilayer_relax_limit_down : int, optional
            Limit relaxation downward to this many lower neighboring layer ids. Use a
            negative value to allow relaxation to any lower layer.
        multilayer_relax_by_jsd : bool, optional
            Weight multilayer relaxation by out-link similarity measured with
            Jensen-Shannon divergence.
        multilayer_relax_to_self : bool, optional
            On relaxation, link a state node to its own physical node in the target
            layer instead of spreading to its out-neighbors. Builds a smaller state
            network with the same flow as the default.
        seed : int, optional
            Set the random number generator seed for reproducible results.
        num_trials : int, optional
            Run this many independent trials and keep the best solution.
        core_loop_limit : int, optional
            Limit how many core loops try to move each node to the best module.
        core_level_limit : int, optional
            Limit how many times core loops are reapplied to the aggregated modular
            network to find larger structures. 0 means no limit.
        tune_iteration_limit : int, optional
            Limit the main iterations in the two-level partition algorithm. 0 means no
            limit.
        core_loop_codelength_threshold : float, optional
            Require at least this codelength improvement to accept a new solution in a
            core loop.
        tune_iteration_relative_threshold : float, optional
            Require each tune iteration to improve codelength by this fraction of the
            initial two-level codelength.
        fast_hierarchical_solution : int, optional
            Find top modules fast. Use 2 to keep all fast levels and 3 to skip the
            recursive part.
        inner_parallelization : bool, optional
            Experimental: use batched parallel node moves for coarse optimization.
            Performance gains are workload-dependent, often require a relaxed
            core-loop-codelength-threshold and low tune-iteration-limit, and may produce
            a different partition than serial optimization.
        parallel_trials : bool, optional
            Run independent trials in parallel with OpenMP. --num-trials remains the
            total number of trials; the number of parallel workers follows the OpenMP
            thread count (e.g. OMP_NUM_THREADS), clamped to --num-trials. Peak memory
            scales with the worker count. Nested OpenMP and --inner-parallelization are
            disabled inside workers.
        converge : bool, optional
            Treat the trial count as a cap and stop early once the best codelength has
            plateaued (no meaningful improvement over several consecutive trials). Runs
            trials serially; cannot be combined with parallel trials or distributed
            sharding. With no explicit trial count, a default cap is used.
        num_threads : str, optional
            Effective thread budget: 'auto' (resolve from --num-threads >
            INFOMAP_NUM_THREADS > SLURM_CPUS_PER_TASK > OMP_NUM_THREADS > cpuset >
            hardware), or a positive integer. 1 forces fully serial. Governs the
            recursive partition, parallel trials, and inner parallelization.
        threads : str, optional
            Alias for --num-threads.
        prefer_modular_solution : bool, optional
            Prefer a modular solution even when one module gives a lower codelength.
        num_random_moves : int, optional
            Try this many random moves in each core loop to merge weakly connected
            nodes.
        max_degree_for_random_moves : int, optional
            Try random moves only for nodes with degree at most this value.
        """
        options = Options._from_locals(locals())
        self._init_from_options(args, options)

    def run(
        self,
        args=None,
        initial_partition=None,
        include_self_links=None,
        skip_adjust_bipartite_flow=False,
        bipartite_teleportation=False,
        weight_threshold=None,
        no_self_links=False,
        node_limit=None,
        matchable_multilayer_ids=None,
        cluster_data=None,
        assign_to_neighbouring_module=False,
        meta_data=None,
        meta_data_rate=1.0,
        meta_data_unweighted=False,
        no_infomap=False,
        out_name=None,
        no_file_output=False,
        tree=False,
        ftree=False,
        clu=False,
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
        trial_offset=None,
        trial_results=None,
        no_final_output=False,
        verbosity_level=1,
        silent=False,
        pretty=False,
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
        preferred_number_of_levels=None,
        preferred_number_of_levels_strength=1.0,
        multilayer_relax_rate=0.15,
        multilayer_relax_limit=-1,
        multilayer_relax_limit_up=-1,
        multilayer_relax_limit_down=-1,
        multilayer_relax_by_jsd=False,
        multilayer_relax_to_self=False,
        seed=123,
        num_trials=1,
        core_loop_limit=10,
        core_level_limit=None,
        tune_iteration_limit=None,
        core_loop_codelength_threshold=1e-10,
        tune_iteration_relative_threshold=1e-05,
        fast_hierarchical_solution=None,
        inner_parallelization=False,
        parallel_trials=False,
        converge=False,
        num_threads=None,
        threads=None,
        prefer_modular_solution=False,
        num_random_moves=None,
        max_degree_for_random_moves=None,
    ):
        """Run Infomap.

        Keyword arguments mirror the Infomap CLI flags. Use
        :class:`Options` for the full parameter reference and
        :func:`infomap.run` with ``options=`` when reusing a saved configuration.

        Parameters
        ----------
        args : str, optional
            Raw Infomap arguments to prepend before rendered keyword options.
        initial_partition : dict, optional
            Initial partition to use for this run only. See initial_partition.
        include_self_links : bool, optional
            Deprecated. Self-links are included by default; use no_self_links=True to
            exclude them.
        skip_adjust_bipartite_flow : bool, optional
            Keep flow on bipartite nodes instead of distributing it to primary nodes.
        bipartite_teleportation : bool, optional
            Use bipartite teleportation instead of the default two-step unipartite
            teleportation.
        weight_threshold : float, optional
            Ignore input links with weight below this threshold.
        no_self_links : bool, optional
            Exclude self-links from the input network.
        node_limit : int, optional
            Read only nodes up to this node id and ignore links connected to higher node
            ids.
        matchable_multilayer_ids : int, optional
            Construct state ids from node ids and layer ids that stay comparable across
            networks. Set at least to the largest layer id among networks to match.
        cluster_data : str, optional
            Read an initial partition from a clu file or a hierarchy from a tree/ftree
            file. Tree input may use physical or state nodes for higher-order networks.
        assign_to_neighbouring_module : bool, optional
            With --cluster-data, assign nodes missing module ids to a neighboring node's
            module when possible.
        meta_data : str, optional
            Read metadata to encode from a clu-format file.
        meta_data_rate : float, optional
            With --meta-data, set the metadata encoding rate. The default encodes
            metadata at each step.
        meta_data_unweighted : bool, optional
            With --meta-data, encode metadata without weighting by node flow.
        no_infomap : bool, optional
            Skip optimization. Use this to calculate codelength for --cluster-data or to
            print non-modular statistics.
        out_name : str, optional
            Base name for output files, for example [out_directory]/[out-name].tree.
        no_file_output : bool, optional
            Do not write output files.
        tree : bool, optional
            Write the modular hierarchy to a tree file. Enabled by default when no other
            output format is selected.
        ftree : bool, optional
            Write the modular hierarchy and aggregated links between nested modules to
            an ftree file. Used by Network Navigator.
        clu : bool, optional
            Write top-level module ids for each node to a clu file.
        clu_level : int, optional
            With --clu or --output clu, write module ids at this depth from the root.
            Use -1 for bottom-level modules.
        output : sequence of str, optional
            Write selected output formats as a comma-separated list without spaces, e.g.
            -o clu,tree,ftree. Options: clu, tree, ftree, newick, json, csv, network,
            states, flow.
        hide_bipartite_nodes : bool, optional
            Hide bipartite nodes in output by projecting the solution to primary nodes.
        print_all_trials : bool, optional
            Write each trial to separate output files. Has effect only when --num-trials
            is greater than 1.
        no_overwrite : bool, optional
            Fail with an output error if any target output file already exists. By
            default existing files are replaced.
        print_config_fingerprint : bool, optional
            Print the canonical configuration fingerprint and exit.
        timing_json : str, optional
            Write machine-readable run timing JSON to this path. Use - for stdout.
        summary_json : str, optional
            Write machine-readable final run summary JSON to this path. Use - for
            stdout.
        manifest_json : str, optional
            Write a machine-readable run manifest JSON to this path. Use - for stdout.
        memory_report : bool, optional
            Include peak RSS and best-effort bytes per node/link estimates in timing
            JSON. Requires --timing-json.
        trial_offset : int, optional
            Global index of the first trial this process runs; trial i uses seed =
            base_seed + (trial_offset + i). Default 0 (single-process behavior).
        trial_results : str, optional
            Write this shard's per-trial results (codelengths, seeds, best-tree
            reference, fingerprints) as JSON to this path, for deterministic merging of
            distributed shard runs into a final solution.
        no_final_output : bool, optional
            Skip writing this process's aggregate best result. Per-trial outputs and
            --trial-results are still written.
        verbosity_level : int, optional
            Verbosity level on the console. 1 keeps the default output level, 2 renders
            -vv and so on.
        silent : bool, optional
            Suppress console output.
        pretty : bool, optional
            Deprecated. Accepted for backward compatibility; has no effect.
        two_level : bool, optional
            Optimize a two-level partition instead of the default multi-level hierarchy.
        flow_model : str, optional
            Choose how Infomap derives flow from the input links. Options: undirected,
            directed, undirdir, outdirdir, rawdir, precomputed.
        directed : bool, optional
            Treat input links as directed. Shorthand for --flow-model directed.
        recorded_teleportation : bool, optional
            When teleportation is used to calculate flow, also record teleportation
            steps in the codelength.
        use_node_weights_as_flow : bool, optional
            Use node weights from the API or Pajek node records as normalized node flow.
        to_nodes : bool, optional
            Teleport to nodes instead of links. Uses uniform node weights unless node
            weights are provided.
        teleportation_probability : float, optional
            Set the probability of teleporting to a random node or link when calculating
            flow.
        regularized : bool, optional
            Add a fully connected Bayesian prior network to reduce overfitting to
            missing links. Activates --recorded-teleportation.
        regularization_strength : float, optional
            Scale the relative strength of the Bayesian prior network used by
            --regularized.
        entropy_corrected : bool, optional
            Correct for negative entropy bias in small samples, especially solutions
            with many modules.
        entropy_correction_strength : float, optional
            Scale the default correction used by --entropy-corrected.
        markov_time : float, optional
            Scale link flow to change the cost of moving between modules. Higher values
            result in fewer modules.
        variable_markov_time : bool, optional
            Vary Markov time locally to reduce overpartitioning in sparse areas while
            keeping higher resolution in dense areas.
        variable_markov_damping : float, optional
            With --variable-markov-time, set damping between local effective degree (0)
            and local entropy (1).
        variable_markov_min_scale : float, optional
            With --variable-markov-time, set the minimum local scale for zero-entropy
            nodes. Local Markov time is max scale divided by local scale.
        preferred_number_of_modules : int, optional
            Penalize solutions by how far their number of modules differs from this
            value.
        preferred_number_of_levels : int, optional
            Soft preference for the depth of the hierarchy. Steering to a shallower
            depth is reliable at a small codelength cost; deeper is best-effort, bounded
            by what the optimizer proposes. No-op with --two-level or strength 0.
        preferred_number_of_levels_strength : float, optional
            Scale the strength of --preferred-number-of-levels. 0 disables the
            preference; larger values increase the cost of deviating from the preferred
            depth.
        multilayer_relax_rate : float, optional
            Set the probability of relaxing from a state node to neighboring layers
            instead of staying in the current layer.
        multilayer_relax_limit : int, optional
            Limit relaxation to this many neighboring layer ids in each direction. Use a
            negative value to allow relaxation to any layer.
        multilayer_relax_limit_up : int, optional
            Limit relaxation upward to this many higher neighboring layer ids. Use a
            negative value to allow relaxation to any higher layer.
        multilayer_relax_limit_down : int, optional
            Limit relaxation downward to this many lower neighboring layer ids. Use a
            negative value to allow relaxation to any lower layer.
        multilayer_relax_by_jsd : bool, optional
            Weight multilayer relaxation by out-link similarity measured with
            Jensen-Shannon divergence.
        multilayer_relax_to_self : bool, optional
            On relaxation, link a state node to its own physical node in the target
            layer instead of spreading to its out-neighbors. Builds a smaller state
            network with the same flow as the default.
        seed : int, optional
            Set the random number generator seed for reproducible results.
        num_trials : int, optional
            Run this many independent trials and keep the best solution.
        core_loop_limit : int, optional
            Limit how many core loops try to move each node to the best module.
        core_level_limit : int, optional
            Limit how many times core loops are reapplied to the aggregated modular
            network to find larger structures. 0 means no limit.
        tune_iteration_limit : int, optional
            Limit the main iterations in the two-level partition algorithm. 0 means no
            limit.
        core_loop_codelength_threshold : float, optional
            Require at least this codelength improvement to accept a new solution in a
            core loop.
        tune_iteration_relative_threshold : float, optional
            Require each tune iteration to improve codelength by this fraction of the
            initial two-level codelength.
        fast_hierarchical_solution : int, optional
            Find top modules fast. Use 2 to keep all fast levels and 3 to skip the
            recursive part.
        inner_parallelization : bool, optional
            Experimental: use batched parallel node moves for coarse optimization.
            Performance gains are workload-dependent, often require a relaxed
            core-loop-codelength-threshold and low tune-iteration-limit, and may produce
            a different partition than serial optimization.
        parallel_trials : bool, optional
            Run independent trials in parallel with OpenMP. --num-trials remains the
            total number of trials; the number of parallel workers follows the OpenMP
            thread count (e.g. OMP_NUM_THREADS), clamped to --num-trials. Peak memory
            scales with the worker count. Nested OpenMP and --inner-parallelization are
            disabled inside workers.
        converge : bool, optional
            Treat the trial count as a cap and stop early once the best codelength has
            plateaued (no meaningful improvement over several consecutive trials). Runs
            trials serially; cannot be combined with parallel trials or distributed
            sharding. With no explicit trial count, a default cap is used.
        num_threads : str, optional
            Effective thread budget: 'auto' (resolve from --num-threads >
            INFOMAP_NUM_THREADS > SLURM_CPUS_PER_TASK > OMP_NUM_THREADS > cpuset >
            hardware), or a positive integer. 1 forces fully serial. Governs the
            recursive partition, parallel trials, and inner parallelization.
        threads : str, optional
            Alias for --num-threads.
        prefer_modular_solution : bool, optional
            Prefer a modular solution even when one module gives a lower codelength.
        num_random_moves : int, optional
            Try this many random moves in each core loop to merge weakly connected
            nodes.
        max_degree_for_random_moves : int, optional
            Try random moves only for nodes with degree at most this value.

        Returns
        -------
        Result
            The result of this run. See :class:`~infomap.Result`.

        See Also
        --------
        initial_partition
        """
        options = Options._from_locals(locals())
        return self._run_from_options(args, initial_partition, options)
    # === END generated ===

    def __repr__(self):
        return _repr_text(self)

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
        """Create an :class:`Infomap` instance from :class:`Options`.

        .. deprecated::
            Pass options to :func:`infomap.run` or :meth:`Infomap.run`
            instead, e.g. ``infomap.run(graph, options=options)``.
        """
        if not isinstance(options, Options):
            raise TypeError("options must be an Options instance")
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
        """Create an :class:`Infomap` instance from a SciPy sparse adjacency matrix.

        .. deprecated::
            Use :meth:`Network.from_scipy_sparse_matrix` or
            ``infomap.run(matrix)``.
        """
        im = cls(args=args, **infomap_options)
        im._add_scipy_sparse_matrix_impl(
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
        """Create an :class:`Infomap` instance from a PyG-style edge index.

        .. deprecated::
            Use :meth:`Network.from_edge_index` or ``infomap.run(edge_index)``.
        """
        im = cls(args=args, **infomap_options)
        im._add_edge_index_impl(
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
        return self._core.readInputData(filename, accumulate)

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
            return self._core.addNode(node_id, name, teleportation_weight)
        elif len(name) and teleportation_weight is None:
            return self._core.addNode(node_id, name)
        elif not len(name) and teleportation_weight is not None:
            return self._core.addNode(node_id, teleportation_weight)

        return self._core.addNode(node_id)

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
        return self._core.addStateNode(state_id, node_id)

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
        return self._core.addName(node_id, name)

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
        return self._core.addLink(source_id, target_id, weight)

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
        return self._core.network().removeLink(source_id, target_id)

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
        return self._core.addMultilayerLink(
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
        return self._core.addMultilayerIntraLink(
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
        return self._core.addMultilayerInterLink(
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
        """Unsupported operation kept for interface parity.

        Raises
        ------
        NotImplementedError
            Always; removing multilayer links is not supported by the
            Python API. Rebuild the network without the link instead.
        """
        return self._network.remove_multilayer_link()

    def set_meta_data(self, node_id, meta_category):
        """Set metadata for a node.

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
        >>> result = im.run(meta_data_rate=0)
        >>> result.num_top_modules
        2
        >>> result = im.run(meta_data_rate=2)
        >>> result.num_top_modules
        3


        Parameters
        ----------
        node_id : int
        meta_category : int
        """
        return self._core.network().addMetaData(node_id, meta_category)

    def add_networkx_graph(
        self,
        g,
        weight="weight",
        node_id="node_id",
        layer_id="layer_id",
        multilayer_inter_intra_format=True,
        meta_attribute=None,
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
        >>> result = im.run()
        >>> for node in result.nodes():
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
        >>> result = im.run()
        >>> for node in result.nodes(states=True):
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
        >>> result = im.run()
        >>> for node in sorted(result.nodes(states=True), key=lambda n: n.state_id):
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
        meta_attribute : str, optional
            Node attribute to read categorical metadata from, for use with
            the meta-data map equation. Values are encoded to integers in
            first-seen order and set as Infomap metadata; nodes with missing
            values are skipped. Raises :class:`ValueError` if the attribute
            is not set on any node.

        Returns
        -------
        dict
            Dict with the internal node ids as keys and original labels as
            values.

        Notes
        -----
        Directedness is auto-detected via ``g.is_directed()`` (see above). The
        graph-library adapters diverge on this: networkx and igraph auto-detect,
        :meth:`add_scipy_sparse_matrix` defaults ``directed=False``, and
        :meth:`add_edge_index` defaults ``directed=True``. They also name their
        weight parameter differently: networkx ``weight``, igraph
        ``edge_weights``, scipy ``weighted`` (bool), edge_index ``edge_weight``.

        Parallel edges in an ``nx.MultiGraph``/``nx.MultiDiGraph`` are each
        forwarded to ``add_link`` and self-loops are passed through.

        .. deprecated::
            Use :meth:`Network.from_networkx` or ``infomap.run(graph)``.
        """
        return self._add_networkx_graph_impl(
            g,
            weight=weight,
            node_id=node_id,
            layer_id=layer_id,
            multilayer_inter_intra_format=multilayer_inter_intra_format,
            meta_attribute=meta_attribute,
        )

    def _add_networkx_graph_impl(
        self,
        g,
        weight="weight",
        node_id="node_id",
        layer_id="layer_id",
        multilayer_inter_intra_format=True,
        meta_attribute=None,
    ):
        mapping = _add_networkx_graph(
            self,
            g,
            weight=weight,
            node_id=node_id,
            layer_id=layer_id,
            multilayer_inter_intra_format=multilayer_inter_intra_format,
            meta_attribute=meta_attribute,
        )
        self.node_id_to_label = mapping
        return mapping

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

        Notes
        -----
        Unlike the networkx/igraph adapters (which auto-detect directedness via
        ``is_directed()``), this adapter defaults ``directed=False`` and names
        its weight control ``weighted`` (a bool). :meth:`add_edge_index` instead
        defaults ``directed=True``.

        .. deprecated::
            Use :meth:`Network.from_scipy_sparse_matrix` or
            ``infomap.run(matrix)``.
        """
        return self._add_scipy_sparse_matrix_impl(
            A, directed=directed, weighted=weighted, node_ids=node_ids
        )

    def _add_scipy_sparse_matrix_impl(
        self, A, directed=False, weighted=True, node_ids=None
    ):
        mapping = _add_scipy_sparse_matrix(
            self,
            A,
            directed=directed,
            weighted=weighted,
            node_ids=node_ids,
        )
        self.node_id_to_label = mapping
        return mapping

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

        Notes
        -----
        Unlike the networkx/igraph adapters (which auto-detect directedness via
        ``is_directed()``), this adapter defaults ``directed=True`` and names its
        weight parameter ``edge_weight``. :meth:`add_scipy_sparse_matrix` instead
        defaults ``directed=False``.

        .. deprecated::
            Use :meth:`Network.from_edge_index` or ``infomap.run(edge_index)``.
        """
        return self._add_edge_index_impl(
            edge_index,
            edge_weight=edge_weight,
            num_nodes=num_nodes,
            directed=directed,
            node_ids=node_ids,
        )

    def _add_edge_index_impl(
        self,
        edge_index,
        edge_weight=None,
        num_nodes=None,
        directed=True,
        node_ids=None,
    ):
        mapping = _add_edge_index(
            self,
            edge_index,
            edge_weight=edge_weight,
            num_nodes=num_nodes,
            directed=directed,
            node_ids=node_ids,
        )
        self.node_id_to_label = mapping
        return mapping

    def add_igraph_graph(
        self,
        g,
        edge_weights=None,
        vertex_weights=None,
        node_id="node_id",
        layer_id="layer_id",
        meta_attribute=None,
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
        meta_attribute : str, optional
            Vertex attribute to read categorical metadata from, for use with
            the meta-data map equation. Values are encoded to integers in
            first-seen order and set as Infomap metadata; vertices with
            missing values are skipped. Raises :class:`ValueError` if the
            attribute does not exist.
        multilayer_inter_intra_format : bool, optional
            Use intra/inter format to simulate inter-layer links. Default
            ``True``.

        Returns
        -------
        dict
            Dict with igraph vertex indices as keys and vertex names as values
            when names are present, otherwise vertex indices as values.

        Notes
        -----
        Directedness is auto-detected via ``g.is_directed()`` (as for networkx).
        The graph-library adapters diverge on this: networkx and igraph
        auto-detect, :meth:`add_scipy_sparse_matrix` defaults ``directed=False``,
        and :meth:`add_edge_index` defaults ``directed=True``. They also name
        their weight parameter differently: igraph ``edge_weights``, networkx
        ``weight``, scipy ``weighted`` (bool), edge_index ``edge_weight``.

        .. deprecated::
            Use :meth:`Network.from_igraph` or ``infomap.run(graph)``.
        """
        return self._add_igraph_graph_impl(
            g,
            edge_weights=edge_weights,
            vertex_weights=vertex_weights,
            node_id=node_id,
            layer_id=layer_id,
            multilayer_inter_intra_format=multilayer_inter_intra_format,
            meta_attribute=meta_attribute,
        )

    def _add_igraph_graph_impl(
        self,
        g,
        edge_weights=None,
        vertex_weights=None,
        node_id="node_id",
        layer_id="layer_id",
        multilayer_inter_intra_format=True,
        meta_attribute=None,
    ):
        mapping = _add_igraph_graph(
            self,
            g,
            edge_weights=edge_weights,
            vertex_weights=vertex_weights,
            node_id=node_id,
            layer_id=layer_id,
            multilayer_inter_intra_format=multilayer_inter_intra_format,
            meta_attribute=meta_attribute,
        )
        self.node_id_to_label = mapping
        return mapping

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
        >>> result = im.run()
        >>> result.codelength
        0.9183


        Parameters
        ----------
        start_id : int
            The node id where the second node type starts.

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
        >>> result = im.run(no_infomap=True)
        >>> result.codelength
        3.4056


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
        return self._core.getInitialPartition()

    @initial_partition.setter
    def initial_partition(self, module_ids):
        if apply_initial_partition(self._core, module_ids):
            # Cache the tuple-keyed form so the getter can round-trip it.
            self._physical_initial_partition = dict(module_ids)
        else:
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

    def _run_from_options(self, args, initial_partition, options):
        args = _package_construct_args()(args, **options.to_kwargs())

        if initial_partition is not None:
            with self._initial_partition(initial_partition):
                self._core.run(args)
        else:
            self._core.run(args)

        # Stamp a fresh Result with the new generation (shared helper). The C++
        # result tree is rebuilt on every run(), so any previously returned
        # Result becomes stale for node-level access (its eager scalars stay
        # valid).
        self._result = build_result(self)
        return self._result

    def run_with_options(self, options, *, args=None, initial_partition=None):
        """Run Infomap using a reusable :class:`Options` instance.

        .. deprecated::
            Use ``infomap.run(input, options=options)`` instead.
        """
        if not isinstance(options, Options):
            raise TypeError("options must be an Options instance")
        return self.run(
            args=args,
            initial_partition=initial_partition,
            **options.to_kwargs(),
        )

    @property
    def network(self):
        """Get the internal network."""
        return self._core.network()

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

        .. deprecated::
            Use ``result = im.run(); result.codelength``.
        """
        return self._core.codelength()

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

        .. deprecated::
            Use ``result = im.run(); result.codelengths``.
        """
        return self._core.codelengths()

    @property
    def elapsed_time(self):
        """Get the elapsed run time in seconds.

        Returns
        -------
        float
            The elapsed run time in seconds.

        .. deprecated::
            Use ``result = im.run(); result.elapsed_time``.
        """
        return self._core.elapsedTime()


def build_info():
    """Report how the compiled Infomap extension was built.

    Returns
    -------
    dict
        A dict with an ``enabled_features`` tuple naming the optional
        features the native engine was compiled with (empty for a standard
        build).

    Examples
    --------
    >>> import infomap
    >>> sorted(infomap.build_info())
    ['enabled_features']
    """
    return _engine_build_info()


def main():
    """Run the ``infomap`` command-line interface.

    This is the console-script entry point behind the ``infomap`` command
    (and ``python -m infomap``): it joins ``sys.argv[1:]`` into a native CLI
    invocation and returns the process exit code, suitable for
    ``sys.exit``. A keyboard interrupt exits cleanly with code 130.
    """
    args = " ".join(sys.argv[1:])
    try:
        return _cli_run(args)
    except KeyboardInterrupt:
        # Ctrl-C during the run: cancelled cooperatively (issue #412). Exit like
        # the native CLI — a clean message and 130 (128 + SIGINT), no traceback.
        print("Interrupted.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
