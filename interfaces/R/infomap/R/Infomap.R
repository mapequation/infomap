#' Infomap network clustering
#'
#' @description
#' Constructor for an Infomap network clustering instance. Mirrors the
#' Python `Infomap` class in shape, with named arguments for every CLI
#' flag and active bindings for read-only properties.
#'
#' @details
#' Build a network by calling `add_node()` / `add_link()` (or pass an
#' existing igraph object to `add_igraph()`), then run with `run()`.
#' Inspect results via the active bindings (`codelength`, `modules`,
#' `num_top_modules`, ...) or use [as.data.frame()] for a tidy
#' node-level frame.
#'
#' Returned objects have R6 class `"Infomap"`; the underlying
#' R6ClassGenerator is exposed as the unexported `InfomapClass` for
#' advanced use (subclassing, method introspection).
#'
#' @param args Optional raw CLI argument string to prepend.
#' @param opts Optional options list from [infomap_options()].
#' @param ... Named option overrides forwarded to [infomap_options()].
#'
#' @return An object of R6 class `"Infomap"`.
#'
#' @examples
#' # Two triangles joined by a bridge.
#' im <- Infomap(silent = TRUE, num_trials = 5)
#' im$add_links(list(c(1, 2), c(1, 3), c(2, 3),
#'                   c(3, 4),
#'                   c(4, 5), c(4, 6), c(5, 6)))
#' im$run()
#' im$num_top_modules
#' im$codelength
#' im$modules
#' @export
Infomap <- function(args = NULL, opts = NULL, ...) {
  InfomapClass$new(args = args, opts = opts, ...)
}

# R6 class generator. Methods are added across multiple files via
# `InfomapClass$set("public", ...)`; roxygen2's R6 integration only
# sees methods defined in the class body, so we skip Rd generation for
# the class itself (via `@noRd`) and document the user-facing API on
# Infomap() instead.
#' @noRd
InfomapClass <- R6::R6Class(
  "Infomap",
  cloneable = FALSE,
  public = list(
    # @description Create a new Infomap instance.
    # @param args Optional raw CLI argument string to prepend.
    # @param opts Optional options list from [infomap_options()].
    # @param ... Named option overrides forwarded to [infomap_options()].
    initialize = function(args = NULL, opts = NULL, ...) {
      overrides <- list(...)
      if (is.null(opts)) {
        opts <- do.call(infomap_options, overrides)
      } else if (length(overrides) > 0L) {
        for (name in names(overrides)) opts[[name]] <- overrides[[name]]
      }
      cli <- construct_args(args, opts)
      private$.swig <- InfomapWrapper(cli)
      private$.flow_model_set <-
        !is.null(opts$flow_model) ||
        !is.null(opts$directed) ||
        .flow_model_in_args(args)
      invisible(self)
    },

    # ----------------------------------------
    # Input
    # ----------------------------------------

    # @description Read network data from file.
    # @param filename Path to a network file (`.net`, `.txt`, `.tree`, etc.).
    # @param accumulate If `TRUE` (default), accumulate to existing nodes/links.
    read_file = function(filename, accumulate = TRUE) {
      private$.swig$readInputData(as.character(filename), as.logical(accumulate))
      invisible(self)
    },

    # @description Add a node.
    # @param node_id Integer node id.
    # @param name Optional node name.
    # @param teleportation_weight Optional teleportation weight.
    add_node = function(node_id, name = NULL, teleportation_weight = NULL) {
      id <- as.integer(node_id)
      if (!is.null(name) && !is.null(teleportation_weight)) {
        private$.swig$addNode(id, as.character(name), as.numeric(teleportation_weight))
      } else if (!is.null(name)) {
        private$.swig$addNode(id, as.character(name))
      } else if (!is.null(teleportation_weight)) {
        private$.swig$addNode(id, as.numeric(teleportation_weight))
      } else {
        private$.swig$addNode(id)
      }
      invisible(self)
    },

    # @description Add many nodes at once.
    # @param nodes A list of integer node ids, or a list of vectors of the form
    #   `c(node_id, name, teleportation_weight)` (last two optional), or a
    #   named list/vector mapping `node_id` to `name` or to
    #   `c(name, teleportation_weight)`.
    add_nodes = function(nodes) {
      if (is.null(names(nodes))) {
        for (entry in nodes) {
          if (length(entry) == 1L && is.numeric(entry)) {
            self$add_node(entry)
          } else {
            args <- as.list(entry)
            do.call(self$add_node, args)
          }
        }
      } else {
        for (id_str in names(nodes)) {
          attr <- nodes[[id_str]]
          id <- as.integer(id_str)
          if (is.character(attr) && length(attr) == 1L) {
            self$add_node(id, name = attr)
          } else if (is.list(attr) || length(attr) > 1L) {
            self$add_node(id, name = as.character(attr[[1]]),
                          teleportation_weight = as.numeric(attr[[2]]))
          } else {
            self$add_node(id, name = as.character(attr))
          }
        }
      }
      invisible(self)
    },

    # @description Add a state node.
    # @param state_id Integer state node id.
    # @param node_id Integer physical node id.
    add_state_node = function(state_id, node_id) {
      private$.swig$addStateNode(as.integer(state_id), as.integer(node_id))
      invisible(self)
    },

    # @description Add many state nodes at once.
    # @param state_nodes A list of `c(state_id, node_id)` vectors, or a
    #   named list/vector mapping `state_id` to `node_id`.
    add_state_nodes = function(state_nodes) {
      if (is.null(names(state_nodes))) {
        for (entry in state_nodes) {
          self$add_state_node(entry[[1L]], entry[[2L]])
        }
      } else {
        for (state_str in names(state_nodes)) {
          self$add_state_node(as.integer(state_str), state_nodes[[state_str]])
        }
      }
      invisible(self)
    },

    # @description Set the name of a node.
    set_name = function(node_id, name) {
      if (is.null(name)) name <- ""
      private$.swig$addName(as.integer(node_id), as.character(name))
      invisible(self)
    },

    # @description Set names for several nodes at once.
    # @param mapping A list of `c(node_id, name)` vectors, or a named
    #   list/vector mapping `node_id` to `name`.
    set_names = function(mapping) {
      if (is.null(names(mapping))) {
        for (entry in mapping) self$set_name(entry[[1L]], entry[[2L]])
      } else {
        for (id_str in names(mapping)) {
          self$set_name(as.integer(id_str), mapping[[id_str]])
        }
      }
      invisible(self)
    },

    # @description Add a link.
    # @param source_id Source node id.
    # @param target_id Target node id.
    # @param weight Link weight.
    add_link = function(source_id, target_id, weight = 1.0) {
      private$.swig$addLink(as.integer(source_id), as.integer(target_id), as.numeric(weight))
      invisible(self)
    },

    # @description Add many links at once.
    # @param links A list of vectors of the form `c(source, target, weight)`
    #   (weight optional), or a 2- or 3-column matrix / data.frame whose
    #   first two columns are integer/numeric node ids and (optionally)
    #   third column is numeric weight.
    add_links = function(links) {
      if (is.matrix(links) || is.data.frame(links)) {
        ncol_links <- ncol(links)
        if (!ncol_links %in% c(2L, 3L)) {
          stop("`links` matrix/data.frame must have 2 or 3 columns ",
               "(source, target, [weight]).", call. = FALSE)
        }
        sources <- if (is.matrix(links)) links[, 1L] else links[[1L]]
        targets <- if (is.matrix(links)) links[, 2L] else links[[2L]]
        if (!is.numeric(sources) || !is.numeric(targets)) {
          stop("`links` source/target columns must be numeric/integer.",
               call. = FALSE)
        }
        weights <- if (ncol_links == 3L) {
          w <- if (is.matrix(links)) links[, 3L] else links[[3L]]
          if (!is.numeric(w)) {
            stop("`links` weight column must be numeric.", call. = FALSE)
          }
          as.numeric(w)
        } else {
          rep(1.0, length(sources))
        }
        for (i in seq_along(sources)) {
          self$add_link(sources[i], targets[i], weights[i])
        }
      } else {
        for (entry in links) {
          do.call(self$add_link, as.list(entry))
        }
      }
      invisible(self)
    },

    # @description Remove a link.
    remove_link = function(source_id, target_id) {
      private$.swig$network()$removeLink(as.integer(source_id), as.integer(target_id))
      invisible(self)
    },

    # @description Remove many links at once.
    remove_links = function(links) {
      for (entry in links) self$remove_link(entry[[1L]], entry[[2L]])
      invisible(self)
    },

    # @description Add a multilayer link.
    # @param source_multilayer_node A `c(layer_id, node_id)` vector or
    #   the result of [multilayer_node()].
    # @param target_multilayer_node A `c(layer_id, node_id)` vector or
    #   the result of [multilayer_node()].
    # @param weight Link weight.
    add_multilayer_link = function(source_multilayer_node, target_multilayer_node, weight = 1.0) {
      src <- as.integer(source_multilayer_node)
      tgt <- as.integer(target_multilayer_node)
      private$.swig$addMultilayerLink(src[[1L]], src[[2L]], tgt[[1L]], tgt[[2L]], as.numeric(weight))
      invisible(self)
    },

    # @description Add an intra-layer link in a multilayer network.
    add_multilayer_intra_link = function(layer_id, source_node_id, target_node_id, weight = 1.0) {
      private$.swig$addMultilayerIntraLink(
        as.integer(layer_id), as.integer(source_node_id),
        as.integer(target_node_id), as.numeric(weight)
      )
      invisible(self)
    },

    # @description Add an inter-layer link in a multilayer network.
    add_multilayer_inter_link = function(source_layer_id, node_id, target_layer_id, weight = 1.0) {
      private$.swig$addMultilayerInterLink(
        as.integer(source_layer_id), as.integer(node_id),
        as.integer(target_layer_id), as.numeric(weight)
      )
      invisible(self)
    },

    # @description Add many multilayer links at once.
    add_multilayer_links = function(links) {
      for (entry in links) {
        weight <- if (length(entry) >= 3L) entry[[3L]] else 1.0
        self$add_multilayer_link(entry[[1L]], entry[[2L]], weight)
      }
      invisible(self)
    },

    # @description Set meta data for a node.
    set_meta_data = function(node_id, meta_category) {
      private$.swig$network()$addMetaData(as.integer(node_id), as.integer(meta_category))
      invisible(self)
    },

    # ----------------------------------------
    # Run
    # ----------------------------------------

    # @description Run Infomap.
    # @param args Optional raw CLI argument string for this run only.
    # @param opts Optional [infomap_options()] result for this run only.
    # @param ... Named option overrides for this run only.
    run = function(args = NULL, opts = NULL, ...) {
      overrides <- list(...)
      if (is.null(opts)) {
        opts <- do.call(infomap_options, overrides)
      } else if (length(overrides) > 0L) {
        for (name in names(overrides)) opts[[name]] <- overrides[[name]]
      }
      # Mirror Python: if add_igraph() saw a directed graph and the user
      # never set a flow model (at construction or here, in opts or args),
      # treat the network as directed.
      if (isTRUE(private$.directed_from_igraph) &&
          !private$.flow_model_set &&
          is.null(opts$flow_model) && is.null(opts$directed) &&
          !.flow_model_in_args(args)) {
        opts$directed <- TRUE
      }
      cli <- construct_args(args, opts)
      private$.swig$run(cli)
      invisible(self)
    },

    # @description Get the bipartite start id (the node id where the second node type starts).
    get_bipartite_start_id = function() {
      private$.swig$network()$bipartiteStartId()
    },

    # @description Set the bipartite start id.
    set_bipartite_start_id = function(start_id) {
      private$.swig$setBipartiteStartId(as.integer(start_id))
      invisible(self)
    },

    # @description Print a short summary.
    print = function(...) {
      cat("<Infomap>\n")
      cat("  num_top_modules:", private$.swig$numTopModules(), "\n")
      cat("  codelength:", format(private$.swig$codelength(), digits = 6), "\n")
      invisible(self)
    }
  ),
  active = list(
    # @field swig The underlying SWIG-generated InfomapWrapper handle.
    swig = function() private$.swig,
    # @field network The underlying Network reference.
    network = function() private$.swig$network(),
    # @field codelength Total (hierarchical) codelength.
    codelength = function() private$.swig$codelength(),
    # @field codelengths Codelength of each trial.
    codelengths = function() {
      v <- private$.swig$codelengths()
      n <- v$size()
      if (n == 0L) return(numeric(0L))
      vapply(seq_len(n) - 1L,
             function(i) v$"__getitem__"(as.integer(i)),
             numeric(1L))
    },
    # @field num_top_modules Number of top modules in the tree.
    num_top_modules = function() private$.swig$numTopModules(),
    # @field num_non_trivial_top_modules Number of non-trivial top modules.
    num_non_trivial_top_modules = function() private$.swig$numNonTrivialTopModules(),
    # @field num_levels Number of levels in the tree.
    num_levels = function() private$.swig$numLevels(),
    # @field max_tree_depth Maximum depth of the tree.
    max_tree_depth = function() private$.swig$maxTreeDepth(),
    # @field num_leaf_nodes Number of leaf nodes in the tree.
    num_leaf_nodes = function() private$.swig$numLeafNodes(),
    # @field num_nodes Number of nodes (state nodes for higher-order networks).
    num_nodes = function() private$.swig$network()$numNodes(),
    # @field num_physical_nodes Number of physical nodes.
    num_physical_nodes = function() private$.swig$network()$numPhysicalNodes(),
    # @field num_links Number of links.
    num_links = function() private$.swig$network()$numLinks(),
    # @field have_memory `TRUE` if this is a state/multilayer network.
    have_memory = function() private$.swig$network()$haveMemoryInput(),
    # @field index_codelength Index codelength (top-level part of the codelength).
    index_codelength = function() private$.swig$getIndexCodelength(),
    # @field module_codelength Module codelength (within-module part).
    module_codelength = function() private$.swig$getModuleCodelength(),
    # @field hierarchical_codelength Hierarchical codelength.
    hierarchical_codelength = function() private$.swig$getHierarchicalCodelength(),
    # @field one_level_codelength One-level codelength baseline.
    one_level_codelength = function() private$.swig$getOneLevelCodelength(),
    # @field relative_codelength_savings Relative codelength savings.
    relative_codelength_savings = function() private$.swig$getRelativeCodelengthSavings(),
    # @field entropy_rate Entropy rate of the network.
    entropy_rate = function() private$.swig$getEntropyRate(),
    # @field max_entropy Maximum possible entropy.
    max_entropy = function() private$.swig$getMaxEntropy(),
    # @field bipartite_start_id Get or set the bipartite start id.
    bipartite_start_id = function(value) {
      if (missing(value)) self$get_bipartite_start_id()
      else self$set_bipartite_start_id(value)
    }
  ),
  private = list(
    .swig = NULL,
    .flow_model_set = FALSE,
    .directed_from_igraph = FALSE
  )
)

# Heuristic: detect whether a raw CLI string already specifies a flow
# model. Imperfect parser (e.g. "--cluster-data --directed" matches
# greedily on -d), but covers the relevant flags. Used to avoid
# overriding user-supplied flags when add_igraph() sees a directed graph.
.flow_model_in_args <- function(args) {
  if (is.null(args) || !nzchar(args)) return(FALSE)
  grepl("(^|\\s)(--directed|-d\\b|--flow-model|-f\\b)", args)
}
