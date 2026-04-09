from __future__ import annotations

import warnings
from dataclasses import dataclass, fields


_DEFAULT_META_DATA_RATE = 1.0
_DEFAULT_VERBOSITY_LEVEL = 1
_DEFAULT_TELEPORTATION_PROB = 0.15
_DEFAULT_MULTILAYER_RELAX_RATE = 0.15
_DEFAULT_SEED = 123
_DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD = 1e-10
_DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD = 1e-5

_INPUT_PRE_SELF_LINK_OPTION_SPECS = (
    ("value", "cluster_data", "--cluster-data", lambda value: value is not None),
    ("flag", "no_infomap", "--no-infomap", None),
    ("flag", "skip_adjust_bipartite_flow", "--skip-adjust-bipartite-flow", None),
    ("flag", "bipartite_teleportation", "--bipartite-teleportation", None),
    ("value", "weight_threshold", "--weight-threshold", lambda value: value is not None),
)

_INPUT_POST_SELF_LINK_OPTION_SPECS = (
    ("value", "node_limit", "--node-limit", lambda value: value is not None),
    (
        "value",
        "matchable_multilayer_ids",
        "--matchable-multilayer-ids",
        lambda value: value is not None,
    ),
    ("flag", "assign_to_neighbouring_module", "--assign-to-neighbouring-module", None),
    ("value", "meta_data", "--meta-data", lambda value: value is not None),
    (
        "value",
        "meta_data_rate",
        "--meta-data-rate",
        lambda value: value != _DEFAULT_META_DATA_RATE,
    ),
    ("flag", "meta_data_unweighted", "--meta-data-unweighted", None),
)

_OUTPUT_PRE_VERBOSITY_OPTION_SPECS = (
    ("flag", "tree", "--tree", None),
    ("flag", "ftree", "--ftree", None),
    ("flag", "clu", "--clu", None),
)

_OUTPUT_POST_VERBOSITY_OPTION_SPECS = (
    ("flag", "silent", "--silent", None),
    ("value", "out_name", "--out-name", lambda value: value is not None),
    ("flag", "no_file_output", "--no-file-output", None),
    ("value", "clu_level", "--clu-level", lambda value: value is not None),
)

_OUTPUT_POST_OUTPUT_OPTION_SPECS = (
    ("flag", "hide_bipartite_nodes", "--hide-bipartite-nodes", None),
    ("flag", "print_all_trials", "--print-all-trials", None),
)

_ALGORITHM_PRE_DIRECTED_OPTION_SPECS = (
    ("flag", "two_level", "--two-level", None),
    ("value", "flow_model", "--flow-model", lambda value: value is not None),
)

_ALGORITHM_POST_DIRECTED_OPTION_SPECS = (
    ("flag", "recorded_teleportation", "--recorded-teleportation", None),
    ("flag", "use_node_weights_as_flow", "--use-node-weights-as-flow", None),
    ("flag", "to_nodes", "--to-nodes", None),
    (
        "value",
        "teleportation_probability",
        "--teleportation-probability",
        lambda value: value != _DEFAULT_TELEPORTATION_PROB,
    ),
    ("flag", "regularized", "--regularized", None),
    (
        "value",
        "regularization_strength",
        "--regularization-strength",
        lambda value: value != 1.0,
    ),
    ("flag", "entropy_corrected", "--entropy-corrected", None),
    (
        "value",
        "entropy_correction_strength",
        "--entropy-correction-strength",
        lambda value: value != 1.0,
    ),
    ("value", "markov_time", "--markov-time", lambda value: value != 1.0),
    ("flag", "variable_markov_time", "--variable-markov-time", None),
    (
        "value",
        "variable_markov_damping",
        "--variable-markov-damping",
        lambda value: value != 1.0,
    ),
    (
        "value",
        "preferred_number_of_modules",
        "--preferred-number-of-modules",
        lambda value: value is not None,
    ),
    (
        "value",
        "multilayer_relax_rate",
        "--multilayer-relax-rate",
        lambda value: value != _DEFAULT_MULTILAYER_RELAX_RATE,
    ),
    (
        "value",
        "multilayer_relax_limit",
        "--multilayer-relax-limit",
        lambda value: value != -1,
    ),
    (
        "value",
        "multilayer_relax_limit_up",
        "--multilayer-relax-limit-up",
        lambda value: value != -1,
    ),
    (
        "value",
        "multilayer_relax_limit_down",
        "--multilayer-relax-limit-down",
        lambda value: value != -1,
    ),
    ("flag", "multilayer_relax_by_jsd", "--multilayer-relax-by-jsd", None),
)

_ACCURACY_PRE_FAST_HIERARCHICAL_OPTION_SPECS = (
    ("value", "seed", "--seed", lambda value: value != _DEFAULT_SEED),
    ("value", "num_trials", "--num-trials", lambda value: value != 1),
    ("value", "core_loop_limit", "--core-loop-limit", lambda value: value != 10),
    ("value", "core_level_limit", "--core-level-limit", lambda value: value is not None),
    (
        "value",
        "tune_iteration_limit",
        "--tune-iteration-limit",
        lambda value: value is not None,
    ),
    (
        "value",
        "core_loop_codelength_threshold",
        "--core-loop-codelength-threshold",
        lambda value: value != _DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD,
    ),
    (
        "value",
        "tune_iteration_relative_threshold",
        "--tune-iteration-relative-threshold",
        lambda value: value != _DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD,
    ),
)

_ACCURACY_POST_FAST_HIERARCHICAL_OPTION_SPECS = (
    ("flag", "prefer_modular_solution", "--prefer-modular-solution", None),
    ("flag", "inner_parallelization", "--inner-parallelization", None),
)


def _append_option_specs(parts, options, specs):
    for option_type, option_name, flag, include in specs:
        value = options[option_name]
        if option_type == "flag":
            if value:
                parts.append(flag)
            continue
        if include(value):
            parts.append(f"{flag} {value}")


def _join_args(base_args, parts):
    if not parts:
        return "" if base_args is None else base_args
    if not base_args:
        return f" {' '.join(parts)}"
    return f"{base_args} {' '.join(parts)}"


@dataclass(slots=True)
class InfomapOptions:
    """Reusable Infomap keyword options.

    This class mirrors the keyword arguments accepted by :class:`infomap.Infomap`
    and :meth:`infomap.Infomap.run`. Use :meth:`to_args` to render command-line
    flags, :meth:`from_mapping` to construct options from existing keyword
    dicts, or the convenience methods :meth:`infomap.Infomap.from_options` and
    :meth:`infomap.Infomap.run_with_options` to apply a reusable configuration.

    Parameters
    ----------
    cluster_data : str, optional
        Provide an initial solution in ``.clu`` or ``.tree``/``.ftree``
        format. On higher-order networks, ``.tree`` input may be either
        physical or state-level.
    no_infomap : bool, optional
        Don't run the optimizer. Useful to calculate codelength of provided
        cluster data or to print non-modular statistics.
    skip_adjust_bipartite_flow : bool, optional
        Skip distributing all flow from the bipartite nodes to the primary
        nodes.
    bipartite_teleportation : bool, optional
        Teleport like the bipartite flow instead of two-step (unipartite)
        teleportation.
    weight_threshold : float, optional
        Ignore input links with less weight than the threshold.
    include_self_links : bool, optional
        Deprecated. Self-links are included by default; use
        ``no_self_links=True`` to exclude them.
    no_self_links : bool, optional
        Exclude self links in the input network.
    node_limit : int, optional
        Ignore links connected to nodes above the limit.
    matchable_multilayer_ids : int, optional
        Construct multilayer state ids that are consistent across networks.
    assign_to_neighbouring_module : bool, optional
        Assign nodes without module assignments from ``cluster_data`` to a
        neighbouring module when possible.
    meta_data : str, optional
        Provide meta data (clu format) that should be encoded.
    meta_data_rate : float, optional
        Metadata encoding rate. Default is to encode each step.
    meta_data_unweighted : bool, optional
        Do not weight meta data by node flow.
    tree : bool, optional
        Write a tree file with the modular hierarchy.
    ftree : bool, optional
        Write an ftree file with aggregated links between modules.
    clu : bool, optional
        Write a clu file with the top cluster ids for each node.
    verbosity_level : int, optional
        Verbosity level on the console. ``1`` keeps the default output level,
        ``2`` renders ``-vv`` and so on.
    silent : bool, optional
        Disable console output.
    out_name : str, optional
        Base name for output files.
    no_file_output : bool, optional
        Disable file output.
    clu_level : int, optional
        Depth to use for clu output. Use ``-1`` for bottom-level modules.
    output : sequence of str, optional
        Output formats to request. Options: ``clu``, ``tree``, ``ftree``,
        ``newick``, ``json``, ``csv``, ``network``, ``states``.
    hide_bipartite_nodes : bool, optional
        Project bipartite solutions to unipartite output.
    print_all_trials : bool, optional
        Print all trials to separate files.
    two_level : bool, optional
        Optimize a two-level partition of the network.
    flow_model : str, optional
        Flow model. Options: ``undirected``, ``directed``, ``undirdir``,
        ``outdirdir``, ``rawdir``, ``precomputed``.
    directed : bool, optional
        Shorthand for ``flow_model="directed"``.
    recorded_teleportation : bool, optional
        Record teleportation when minimizing codelength.
    use_node_weights_as_flow : bool, optional
        Use node weights as flow and normalize them to sum to 1.
    to_nodes : bool, optional
        Teleport to nodes instead of links.
    teleportation_probability : float, optional
        Probability of teleporting to a random node or link.
    regularized : bool, optional
        Add a fully connected Bayesian prior network to reduce overfitting.
    regularization_strength : float, optional
        Strength multiplier for the Bayesian prior network.
    entropy_corrected : bool, optional
        Correct for negative entropy bias in small samples.
    entropy_correction_strength : float, optional
        Strength multiplier for entropy correction.
    markov_time : float, optional
        Scale link flow to change the cost of moving between modules.
    variable_markov_time : bool, optional
        Increase Markov time locally to level out link flow.
    variable_markov_damping : float, optional
        Exponent for variable Markov time scale. ``0`` means no rescaling and
        ``1`` means full rescaling to constant transition flow rate.
    preferred_number_of_modules : int, optional
        Penalize solutions by how much they differ from this number.
    multilayer_relax_rate : float, optional
        Probability to relax the constraint to move only in the current layer.
    multilayer_relax_limit : int, optional
        Number of neighbouring layers in each direction to relax to. Negative
        values relax to any layer.
    multilayer_relax_limit_up : int, optional
        Number of neighbouring layers with higher ids to relax to.
    multilayer_relax_limit_down : int, optional
        Number of neighbouring layers with lower ids to relax to.
    multilayer_relax_by_jsd : bool, optional
        Relax proportional to out-link similarity measured by Jensen-Shannon
        divergence.
    seed : int, optional
        Random seed for reproducible results.
    num_trials : int, optional
        Number of outer-most loops to run before picking the best solution.
    core_loop_limit : int, optional
        Limit the number of core loops.
    core_level_limit : int, optional
        Limit the number of times core loops are reapplied on the modular
        network.
    tune_iteration_limit : int, optional
        Limit the number of main iterations in the two-level partition
        algorithm. ``0`` means no limit.
    core_loop_codelength_threshold : float, optional
        Minimum codelength threshold for accepting a new core-loop solution.
    tune_iteration_relative_threshold : float, optional
        Codelength improvement threshold for each tune iteration relative to
        the initial two-level codelength.
    fast_hierarchical_solution : int, optional
        Find top modules fast. Use ``2`` to keep all fast levels and ``3`` to
        skip the recursive part.
    prefer_modular_solution : bool, optional
        Prefer modular solutions even if they are worse than the one-module
        solution.
    inner_parallelization : bool, optional
        Parallelize the inner-most loop for greater speed with some possible
        accuracy tradeoff.
    """

    # input
    cluster_data: str | None = None
    no_infomap: bool = False
    skip_adjust_bipartite_flow: bool = False
    bipartite_teleportation: bool = False
    weight_threshold: float | None = None
    include_self_links: bool | None = None
    no_self_links: bool = False
    node_limit: int | None = None
    matchable_multilayer_ids: int | None = None
    assign_to_neighbouring_module: bool = False
    meta_data: str | None = None
    meta_data_rate: float = _DEFAULT_META_DATA_RATE
    meta_data_unweighted: bool = False
    # output
    tree: bool = False
    ftree: bool = False
    clu: bool = False
    verbosity_level: int = _DEFAULT_VERBOSITY_LEVEL
    silent: bool = False
    out_name: str | None = None
    no_file_output: bool = False
    clu_level: int | None = None
    output: list[str] | tuple[str, ...] | None = None
    hide_bipartite_nodes: bool = False
    print_all_trials: bool = False
    # algorithm
    two_level: bool = False
    flow_model: str | None = None
    directed: bool | None = None
    recorded_teleportation: bool = False
    use_node_weights_as_flow: bool = False
    to_nodes: bool = False
    teleportation_probability: float = _DEFAULT_TELEPORTATION_PROB
    regularized: bool = False
    regularization_strength: float = 1.0
    entropy_corrected: bool = False
    entropy_correction_strength: float = 1.0
    markov_time: float = 1.0
    variable_markov_time: bool = False
    variable_markov_damping: float = 1.0
    preferred_number_of_modules: int | None = None
    multilayer_relax_rate: float = _DEFAULT_MULTILAYER_RELAX_RATE
    multilayer_relax_limit: int = -1
    multilayer_relax_limit_up: int = -1
    multilayer_relax_limit_down: int = -1
    multilayer_relax_by_jsd: bool = False
    # accuracy
    seed: int = _DEFAULT_SEED
    num_trials: int = 1
    core_loop_limit: int = 10
    core_level_limit: int | None = None
    tune_iteration_limit: int | None = None
    core_loop_codelength_threshold: float = _DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD
    tune_iteration_relative_threshold: float = _DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD
    fast_hierarchical_solution: int | None = None
    prefer_modular_solution: bool = False
    inner_parallelization: bool = False

    @classmethod
    def from_mapping(cls, mapping):
        return cls(**{name: mapping[name] for name in _OPTION_FIELD_NAMES if name in mapping})

    def to_kwargs(self):
        return {name: getattr(self, name) for name in _OPTION_FIELD_NAMES}

    def to_args(self, base_args: str | None = None):
        options = self.to_kwargs()
        rendered_args = []

        if self.include_self_links is not None:
            warnings.warn(
                "include_self_links is deprecated, use no_self_links to exclude self-links",
                DeprecationWarning,
                stacklevel=2,
            )

        _append_option_specs(rendered_args, options, _INPUT_PRE_SELF_LINK_OPTION_SPECS)

        if (self.include_self_links is not None and not self.include_self_links) or self.no_self_links:
            rendered_args.append("--no-self-links")

        _append_option_specs(rendered_args, options, _INPUT_POST_SELF_LINK_OPTION_SPECS)
        _append_option_specs(rendered_args, options, _OUTPUT_PRE_VERBOSITY_OPTION_SPECS)

        if self.verbosity_level > 1:
            rendered_args.append(f"-{'v' * self.verbosity_level}")

        _append_option_specs(rendered_args, options, _OUTPUT_POST_VERBOSITY_OPTION_SPECS)
        if self.output is not None:
            rendered_args.append(f"--output {','.join(self.output)}")
        _append_option_specs(rendered_args, options, _OUTPUT_POST_OUTPUT_OPTION_SPECS)
        _append_option_specs(rendered_args, options, _ALGORITHM_PRE_DIRECTED_OPTION_SPECS)

        if self.directed is not None:
            rendered_args.append("--directed" if self.directed else "--flow-model undirected")
        _append_option_specs(rendered_args, options, _ALGORITHM_POST_DIRECTED_OPTION_SPECS)
        _append_option_specs(rendered_args, options, _ACCURACY_PRE_FAST_HIERARCHICAL_OPTION_SPECS)

        if self.fast_hierarchical_solution is not None:
            rendered_args.append(f"-{'F' * self.fast_hierarchical_solution}")

        _append_option_specs(rendered_args, options, _ACCURACY_POST_FAST_HIERARCHICAL_OPTION_SPECS)
        return _join_args(base_args, rendered_args)


_OPTION_FIELD_NAMES = tuple(field.name for field in fields(InfomapOptions))


def _construct_args(
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
    meta_data_rate=_DEFAULT_META_DATA_RATE,
    meta_data_unweighted=False,
    # output
    tree=False,
    ftree=False,
    clu=False,
    verbosity_level=_DEFAULT_VERBOSITY_LEVEL,
    silent=False,
    out_name=None,
    no_file_output=False,
    clu_level=None,
    output=None,
    hide_bipartite_nodes=False,
    print_all_trials=False,
    # algorithm
    two_level=False,
    flow_model=None,
    directed=None,
    recorded_teleportation=False,
    use_node_weights_as_flow=False,
    to_nodes=False,
    teleportation_probability=_DEFAULT_TELEPORTATION_PROB,
    regularized=False,
    regularization_strength=1.0,
    entropy_corrected=False,
    entropy_correction_strength=1.0,
    markov_time=1.0,
    variable_markov_time=False,
    variable_markov_damping=1.0,
    preferred_number_of_modules=None,
    multilayer_relax_rate=_DEFAULT_MULTILAYER_RELAX_RATE,
    multilayer_relax_limit=-1,
    multilayer_relax_limit_up=-1,
    multilayer_relax_limit_down=-1,
    multilayer_relax_by_jsd=False,
    # accuracy
    seed=_DEFAULT_SEED,
    num_trials=1,
    core_loop_limit=10,
    core_level_limit=None,
    tune_iteration_limit=None,
    core_loop_codelength_threshold=_DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD,
    tune_iteration_relative_threshold=_DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD,
    fast_hierarchical_solution=None,
    prefer_modular_solution=False,
    inner_parallelization=False,
):
    return InfomapOptions.from_mapping(locals()).to_args(base_args=args)
