# Defaults, option specs, and CLI argument construction.
# Translated from interfaces/python/src/infomap/_options.py.

DEFAULT_META_DATA_RATE <- 1.0
DEFAULT_VERBOSITY_LEVEL <- 1L
DEFAULT_TELEPORTATION_PROB <- 0.15
DEFAULT_MULTILAYER_RELAX_RATE <- 0.15
DEFAULT_SEED <- 123L
DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD <- 1e-10
DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD <- 1e-5

# Each option is described as a list:
#   list(type = "flag" | "value", name = R-side keyword, flag = CLI flag,
#        include = function(value) -> logical, default = R default)
# Option order matches the rendering order in _options.py so the resulting
# CLI string matches the Python facade's output character-for-character
# (modulo whitespace).

.skip_when_null <- function(x) !is.null(x)
.skip_when_not_equal <- function(default) function(x) !identical(x, default)

INPUT_PRE_SELF_LINK_OPTIONS <- list(
  list(type = "value", name = "cluster_data", flag = "--cluster-data", default = NULL, include = .skip_when_null),
  list(type = "flag",  name = "no_infomap", flag = "--no-infomap", default = FALSE),
  list(type = "flag",  name = "skip_adjust_bipartite_flow", flag = "--skip-adjust-bipartite-flow", default = FALSE),
  list(type = "flag",  name = "bipartite_teleportation", flag = "--bipartite-teleportation", default = FALSE),
  list(type = "value", name = "weight_threshold", flag = "--weight-threshold", default = NULL, include = .skip_when_null)
)

INPUT_POST_SELF_LINK_OPTIONS <- list(
  list(type = "value", name = "node_limit", flag = "--node-limit", default = NULL, include = .skip_when_null),
  list(type = "value", name = "matchable_multilayer_ids", flag = "--matchable-multilayer-ids", default = NULL, include = .skip_when_null),
  list(type = "flag",  name = "assign_to_neighbouring_module", flag = "--assign-to-neighbouring-module", default = FALSE),
  list(type = "value", name = "meta_data", flag = "--meta-data", default = NULL, include = .skip_when_null),
  list(type = "value", name = "meta_data_rate", flag = "--meta-data-rate", default = DEFAULT_META_DATA_RATE, include = .skip_when_not_equal(DEFAULT_META_DATA_RATE)),
  list(type = "flag",  name = "meta_data_unweighted", flag = "--meta-data-unweighted", default = FALSE)
)

OUTPUT_PRE_VERBOSITY_OPTIONS <- list(
  list(type = "flag", name = "tree", flag = "--tree", default = FALSE),
  list(type = "flag", name = "ftree", flag = "--ftree", default = FALSE),
  list(type = "flag", name = "clu", flag = "--clu", default = FALSE)
)

OUTPUT_POST_VERBOSITY_OPTIONS <- list(
  list(type = "flag",  name = "silent", flag = "--silent", default = FALSE),
  list(type = "value", name = "out_name", flag = "--out-name", default = NULL, include = .skip_when_null),
  list(type = "flag",  name = "no_file_output", flag = "--no-file-output", default = FALSE),
  list(type = "value", name = "clu_level", flag = "--clu-level", default = NULL, include = .skip_when_null)
)

OUTPUT_POST_OUTPUT_OPTIONS <- list(
  list(type = "flag", name = "hide_bipartite_nodes", flag = "--hide-bipartite-nodes", default = FALSE),
  list(type = "flag", name = "print_all_trials", flag = "--print-all-trials", default = FALSE)
)

ALGORITHM_PRE_DIRECTED_OPTIONS <- list(
  list(type = "flag",  name = "two_level", flag = "--two-level", default = FALSE),
  list(type = "value", name = "flow_model", flag = "--flow-model", default = NULL, include = .skip_when_null)
)

ALGORITHM_POST_DIRECTED_OPTIONS <- list(
  list(type = "flag",  name = "recorded_teleportation", flag = "--recorded-teleportation", default = FALSE),
  list(type = "flag",  name = "use_node_weights_as_flow", flag = "--use-node-weights-as-flow", default = FALSE),
  list(type = "flag",  name = "to_nodes", flag = "--to-nodes", default = FALSE),
  list(type = "value", name = "teleportation_probability", flag = "--teleportation-probability",
       default = DEFAULT_TELEPORTATION_PROB, include = .skip_when_not_equal(DEFAULT_TELEPORTATION_PROB)),
  list(type = "flag",  name = "regularized", flag = "--regularized", default = FALSE),
  list(type = "value", name = "regularization_strength", flag = "--regularization-strength",
       default = 1.0, include = .skip_when_not_equal(1.0)),
  list(type = "flag",  name = "entropy_corrected", flag = "--entropy-corrected", default = FALSE),
  list(type = "value", name = "entropy_correction_strength", flag = "--entropy-correction-strength",
       default = 1.0, include = .skip_when_not_equal(1.0)),
  list(type = "value", name = "markov_time", flag = "--markov-time", default = 1.0, include = .skip_when_not_equal(1.0)),
  list(type = "flag",  name = "variable_markov_time", flag = "--variable-markov-time", default = FALSE),
  list(type = "value", name = "variable_markov_damping", flag = "--variable-markov-damping",
       default = 1.0, include = .skip_when_not_equal(1.0)),
  list(type = "value", name = "preferred_number_of_modules", flag = "--preferred-number-of-modules",
       default = NULL, include = .skip_when_null),
  list(type = "value", name = "multilayer_relax_rate", flag = "--multilayer-relax-rate",
       default = DEFAULT_MULTILAYER_RELAX_RATE, include = .skip_when_not_equal(DEFAULT_MULTILAYER_RELAX_RATE)),
  list(type = "value", name = "multilayer_relax_limit", flag = "--multilayer-relax-limit",
       default = NULL, include = .skip_when_null),
  list(type = "value", name = "multilayer_relax_limit_up", flag = "--multilayer-relax-limit-up",
       default = NULL, include = .skip_when_null),
  list(type = "value", name = "multilayer_relax_limit_down", flag = "--multilayer-relax-limit-down",
       default = NULL, include = .skip_when_null),
  list(type = "flag",  name = "multilayer_relax_by_jsd", flag = "--multilayer-relax-by-jsd", default = FALSE)
)

ACCURACY_PRE_FAST_HIERARCHICAL_OPTIONS <- list(
  list(type = "value", name = "seed", flag = "--seed", default = DEFAULT_SEED, include = .skip_when_not_equal(DEFAULT_SEED)),
  list(type = "value", name = "num_trials", flag = "--num-trials", default = 1L, include = .skip_when_not_equal(1L)),
  list(type = "value", name = "core_loop_limit", flag = "--core-loop-limit", default = 10L, include = .skip_when_not_equal(10L)),
  list(type = "value", name = "core_level_limit", flag = "--core-level-limit", default = NULL, include = .skip_when_null),
  list(type = "value", name = "tune_iteration_limit", flag = "--tune-iteration-limit", default = NULL, include = .skip_when_null),
  list(type = "value", name = "core_loop_codelength_threshold", flag = "--core-loop-codelength-threshold",
       default = DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD,
       include = .skip_when_not_equal(DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD)),
  list(type = "value", name = "tune_iteration_relative_threshold", flag = "--tune-iteration-relative-threshold",
       default = DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD,
       include = .skip_when_not_equal(DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD))
)

ACCURACY_POST_FAST_HIERARCHICAL_OPTIONS <- list(
  list(type = "flag", name = "prefer_modular_solution", flag = "--prefer-modular-solution", default = FALSE),
  list(type = "flag", name = "inner_parallelization", flag = "--inner-parallelization", default = FALSE)
)

# Order-independent fields not driven by the spec tables: include_self_links,
# no_self_links, verbosity_level, output, directed, fast_hierarchical_solution.
# These are rendered by hand in build_args() to match the Python output order.

OPTION_FIELD_NAMES <- c(
  # input
  "cluster_data", "no_infomap", "skip_adjust_bipartite_flow", "bipartite_teleportation",
  "weight_threshold", "include_self_links", "no_self_links",
  "node_limit", "matchable_multilayer_ids", "assign_to_neighbouring_module",
  "meta_data", "meta_data_rate", "meta_data_unweighted",
  # output
  "tree", "ftree", "clu", "verbosity_level", "silent", "out_name", "no_file_output",
  "clu_level", "output", "hide_bipartite_nodes", "print_all_trials",
  # algorithm
  "two_level", "flow_model", "directed", "recorded_teleportation",
  "use_node_weights_as_flow", "to_nodes", "teleportation_probability",
  "regularized", "regularization_strength", "entropy_corrected",
  "entropy_correction_strength", "markov_time", "variable_markov_time",
  "variable_markov_damping", "preferred_number_of_modules",
  "multilayer_relax_rate", "multilayer_relax_limit", "multilayer_relax_limit_up",
  "multilayer_relax_limit_down", "multilayer_relax_by_jsd",
  # accuracy
  "seed", "num_trials", "core_loop_limit", "core_level_limit", "tune_iteration_limit",
  "core_loop_codelength_threshold", "tune_iteration_relative_threshold",
  "fast_hierarchical_solution", "prefer_modular_solution", "inner_parallelization"
)

OPTION_DEFAULTS <- list(
  cluster_data = NULL, no_infomap = FALSE, skip_adjust_bipartite_flow = FALSE,
  bipartite_teleportation = FALSE, weight_threshold = NULL,
  include_self_links = NULL, no_self_links = FALSE,
  node_limit = NULL, matchable_multilayer_ids = NULL,
  assign_to_neighbouring_module = FALSE, meta_data = NULL,
  meta_data_rate = DEFAULT_META_DATA_RATE, meta_data_unweighted = FALSE,
  tree = FALSE, ftree = FALSE, clu = FALSE,
  verbosity_level = DEFAULT_VERBOSITY_LEVEL, silent = FALSE,
  out_name = NULL, no_file_output = FALSE, clu_level = NULL,
  output = NULL, hide_bipartite_nodes = FALSE, print_all_trials = FALSE,
  two_level = FALSE, flow_model = NULL, directed = NULL,
  recorded_teleportation = FALSE, use_node_weights_as_flow = FALSE,
  to_nodes = FALSE, teleportation_probability = DEFAULT_TELEPORTATION_PROB,
  regularized = FALSE, regularization_strength = 1.0,
  entropy_corrected = FALSE, entropy_correction_strength = 1.0,
  markov_time = 1.0, variable_markov_time = FALSE, variable_markov_damping = 1.0,
  preferred_number_of_modules = NULL,
  multilayer_relax_rate = DEFAULT_MULTILAYER_RELAX_RATE,
  multilayer_relax_limit = NULL, multilayer_relax_limit_up = NULL,
  multilayer_relax_limit_down = NULL, multilayer_relax_by_jsd = FALSE,
  seed = DEFAULT_SEED, num_trials = 1L, core_loop_limit = 10L,
  core_level_limit = NULL, tune_iteration_limit = NULL,
  core_loop_codelength_threshold = DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD,
  tune_iteration_relative_threshold = DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD,
  fast_hierarchical_solution = NULL, prefer_modular_solution = FALSE,
  inner_parallelization = FALSE
)

#' Build a reusable Infomap options list
#'
#' Returns a named list with one entry per Infomap CLI option. All
#' arguments default to the Infomap-internal defaults, so callers only
#' need to supply the options they want to override. The returned list
#' is accepted by [Infomap()] and `Infomap$run()`.
#'
#' Argument names mirror the Infomap CLI flags (with hyphens replaced by
#' underscores) and the keyword arguments accepted by the Python
#' interface.
#'
#' @param ... Named option overrides. See **Options** below.
#'
#' @section Options:
#'
#' Input
#' \describe{
#'   \item{`cluster_data`}{Path to an initial two-level (`.clu`) or
#'     multi-layer (`.tree`) solution.}
#'   \item{`no_infomap`}{If `TRUE`, do not run the optimizer. Useful for
#'     calculating codelength of provided cluster data or for
#'     non-modular statistics.}
#'   \item{`skip_adjust_bipartite_flow`}{Skip distributing all flow from
#'     the bipartite nodes to the primary nodes.}
#'   \item{`bipartite_teleportation`}{Teleport like the bipartite flow
#'     instead of two-step (unipartite) teleportation.}
#'   \item{`weight_threshold`}{Ignore input links with less weight than
#'     the threshold.}
#'   \item{`include_self_links`}{Deprecated. Self-links are included by
#'     default; use `no_self_links = TRUE` to exclude them.}
#'   \item{`no_self_links`}{Exclude self links in the input network.}
#'   \item{`node_limit`}{Ignore links connected to nodes above the limit.}
#'   \item{`matchable_multilayer_ids`}{Construct multilayer state ids
#'     that are consistent across networks.}
#'   \item{`assign_to_neighbouring_module`}{Assign nodes without module
#'     assignments from `cluster_data` to a neighbouring module when
#'     possible.}
#'   \item{`meta_data`}{Path to a `.clu` file with metadata to encode.}
#'   \item{`meta_data_rate`}{Metadata encoding rate. Default is to
#'     encode each step.}
#'   \item{`meta_data_unweighted`}{Do not weight meta data by node flow.}
#' }
#'
#' Output
#' \describe{
#'   \item{`tree`, `ftree`, `clu`}{Write a tree, ftree, or clu file
#'     respectively.}
#'   \item{`verbosity_level`}{Console verbosity level. `1` keeps the
#'     default; `2` renders `-vv`, etc.}
#'   \item{`silent`}{Disable console output.}
#'   \item{`out_name`}{Base name for output files.}
#'   \item{`no_file_output`}{Disable file output.}
#'   \item{`clu_level`}{Depth used for clu output. Use `-1` for
#'     bottom-level modules.}
#'   \item{`output`}{Character vector of output formats. One or more of
#'     `"clu"`, `"tree"`, `"ftree"`, `"newick"`, `"json"`, `"csv"`,
#'     `"network"`, `"states"`.}
#'   \item{`hide_bipartite_nodes`}{Project bipartite solutions to
#'     unipartite output.}
#'   \item{`print_all_trials`}{Print all trials to separate files.}
#' }
#'
#' Algorithm
#' \describe{
#'   \item{`two_level`}{Optimize a two-level partition of the network.}
#'   \item{`flow_model`}{One of `"undirected"`, `"directed"`,
#'     `"undirdir"`, `"outdirdir"`, `"rawdir"`, `"precomputed"`.}
#'   \item{`directed`}{Shorthand for `flow_model = "directed"`.}
#'   \item{`recorded_teleportation`}{Record teleportation when
#'     minimizing codelength.}
#'   \item{`use_node_weights_as_flow`}{Use node weights as flow and
#'     normalize them to sum to 1.}
#'   \item{`to_nodes`}{Teleport to nodes instead of links.}
#'   \item{`teleportation_probability`}{Probability of teleporting to a
#'     random node or link.}
#'   \item{`regularized`, `regularization_strength`}{Add a fully
#'     connected Bayesian prior network to reduce overfitting.}
#'   \item{`entropy_corrected`, `entropy_correction_strength`}{Correct
#'     for negative entropy bias in small samples.}
#'   \item{`markov_time`, `variable_markov_time`,
#'     `variable_markov_damping`}{Scale link flow to change the cost of
#'     moving between modules.}
#'   \item{`preferred_number_of_modules`}{Penalize solutions by how much
#'     they differ from this number.}
#'   \item{`multilayer_relax_rate`, `multilayer_relax_limit`,
#'     `multilayer_relax_limit_up`, `multilayer_relax_limit_down`,
#'     `multilayer_relax_by_jsd`}{Multilayer relaxation parameters.}
#' }
#'
#' Accuracy
#' \describe{
#'   \item{`seed`}{Random seed for reproducible results.}
#'   \item{`num_trials`}{Number of outer-most loops to run before
#'     picking the best solution.}
#'   \item{`core_loop_limit`, `core_level_limit`,
#'     `tune_iteration_limit`}{Iteration limits.}
#'   \item{`core_loop_codelength_threshold`,
#'     `tune_iteration_relative_threshold`}{Convergence thresholds.}
#'   \item{`fast_hierarchical_solution`}{Find top modules fast. Use `2`
#'     to keep all fast levels and `3` to skip the recursive part.}
#'   \item{`prefer_modular_solution`}{Prefer modular solutions even if
#'     they are worse than the one-module solution.}
#'   \item{`inner_parallelization`}{Parallelize the inner-most loop for
#'     greater speed with some possible accuracy tradeoff.}
#' }
#'
#' @return A named list of options.
#' @examples
#' opts <- infomap_options(num_trials = 10, two_level = TRUE)
#' im <- Infomap(opts = opts, silent = TRUE)
#' @export
infomap_options <- function(...) {
  overrides <- list(...)
  unknown <- setdiff(names(overrides), OPTION_FIELD_NAMES)
  if (length(unknown) > 0L) {
    stop(
      "Unknown infomap options: ", paste(unknown, collapse = ", "),
      ". See ?infomap_options for the full list.",
      call. = FALSE
    )
  }
  opts <- OPTION_DEFAULTS
  for (name in names(overrides)) {
    opts[[name]] <- overrides[[name]]
  }
  opts
}

.append_specs <- function(parts, opts, specs) {
  for (spec in specs) {
    value <- opts[[spec$name]]
    if (identical(spec$type, "flag")) {
      if (isTRUE(value)) parts <- c(parts, spec$flag)
      next
    }
    include <- if (is.null(spec$include)) function(v) !is.null(v) && !identical(v, spec$default) else spec$include
    if (isTRUE(include(value))) {
      parts <- c(parts, paste(spec$flag, format_value(value)))
    }
  }
  parts
}

format_value <- function(value) {
  if (is.character(value)) {
    return(value)
  }
  if (is.logical(value)) {
    return(if (isTRUE(value)) "true" else "false")
  }
  if (is.numeric(value)) {
    if (is.integer(value) || (is.finite(value) && value == as.integer(value))) {
      return(format(as.integer(value), scientific = FALSE))
    }
    return(format(value, scientific = FALSE, trim = TRUE))
  }
  as.character(value)
}

#' Render an Infomap options list to a CLI argument string
#'
#' This is exported for advanced use; most callers should pass options
#' directly to [Infomap()] or `Infomap$run()`.
#'
#' @param args Optional raw argument string to prepend.
#' @param opts Options list from [infomap_options()] (or `NULL`).
#'
#' @return A single character string of CLI arguments.
#' @export
construct_args <- function(args = NULL, opts = NULL) {
  if (is.null(opts)) opts <- OPTION_DEFAULTS
  parts <- character(0)

  if (!is.null(opts$include_self_links)) {
    .Deprecated(
      new = "no_self_links",
      package = "infomap",
      old = "include_self_links",
      msg = "include_self_links is deprecated; use no_self_links = TRUE to exclude self-links."
    )
  }

  parts <- .append_specs(parts, opts, INPUT_PRE_SELF_LINK_OPTIONS)

  if ((!is.null(opts$include_self_links) && !isTRUE(opts$include_self_links)) ||
      isTRUE(opts$no_self_links)) {
    parts <- c(parts, "--no-self-links")
  }

  parts <- .append_specs(parts, opts, INPUT_POST_SELF_LINK_OPTIONS)
  parts <- .append_specs(parts, opts, OUTPUT_PRE_VERBOSITY_OPTIONS)

  if (isTRUE(opts$verbosity_level > 1L)) {
    parts <- c(parts, paste0("-", strrep("v", opts$verbosity_level)))
  }

  parts <- .append_specs(parts, opts, OUTPUT_POST_VERBOSITY_OPTIONS)
  if (!is.null(opts$output)) {
    parts <- c(parts, paste("--output", paste(opts$output, collapse = ",")))
  }
  parts <- .append_specs(parts, opts, OUTPUT_POST_OUTPUT_OPTIONS)
  parts <- .append_specs(parts, opts, ALGORITHM_PRE_DIRECTED_OPTIONS)

  if (!is.null(opts$directed)) {
    parts <- c(parts, if (isTRUE(opts$directed)) "--directed" else "--flow-model undirected")
  }
  parts <- .append_specs(parts, opts, ALGORITHM_POST_DIRECTED_OPTIONS)
  parts <- .append_specs(parts, opts, ACCURACY_PRE_FAST_HIERARCHICAL_OPTIONS)

  if (!is.null(opts$fast_hierarchical_solution)) {
    parts <- c(parts, paste0("-", strrep("F", opts$fast_hierarchical_solution)))
  }

  parts <- .append_specs(parts, opts, ACCURACY_POST_FAST_HIERARCHICAL_OPTIONS)

  rendered <- paste(parts, collapse = " ")
  if (is.null(args) || !nzchar(args)) {
    rendered
  } else if (length(parts) == 0L) {
    args
  } else {
    paste(args, rendered)
  }
}
