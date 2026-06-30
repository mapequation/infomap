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
#' R6ClassGenerator is also exported as `InfomapClass` for advanced use
#' (subclassing, method introspection).
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

.network_input_columns <- function(input, columns) {
  if (is.matrix(input)) {
    lapply(columns, function(index) input[, index])
  } else {
    lapply(columns, function(index) input[[index]])
  }
}

.network_input_require_numeric <- function(columns, message) {
  if (!all(vapply(columns, is.numeric, logical(1L)))) {
    stop(message, call. = FALSE)
  }
}

.network_input_weights <- function(input, weight_index, count, message) {
  if (ncol(input) == weight_index) {
    weights <- if (is.matrix(input)) {
      input[, weight_index]
    } else {
      input[[weight_index]]
    }
    if (!is.numeric(weights)) {
      stop(message, call. = FALSE)
    }
    as.numeric(weights)
  } else {
    rep(1.0, count)
  }
}

.network_input_validate_entry_lengths <- function(entries, allowed, message) {
  lengths <- vapply(entries, length, integer(1L))
  if (!all(lengths %in% allowed)) {
    stop(message, call. = FALSE)
  }
  lengths
}

.network_input_scalar_numeric <- function(
  entry,
  index,
  scalar_message,
  numeric_message
) {
  value <- entry[[index]]
  if (length(value) != 1L) {
    stop(scalar_message, call. = FALSE)
  }
  if (!is.numeric(value)) {
    stop(numeric_message, call. = FALSE)
  }
  value
}

.network_input_table <- function(
  input,
  allowed_columns,
  shape_message,
  id_columns,
  id_message,
  weight_column,
  weight_message
) {
  ncol_input <- ncol(input)
  if (!ncol_input %in% allowed_columns) {
    stop(shape_message, call. = FALSE)
  }

  values <- .network_input_columns(input, id_columns)
  .network_input_require_numeric(values, id_message)
  weights <- .network_input_weights(
    input,
    weight_column,
    length(values[[1L]]),
    weight_message
  )
  c(values, list(weights))
}

.network_input_flat_rows <- function(
  entries,
  allowed_lengths,
  length_message,
  value_names,
  row_name,
  id_message,
  weight_scalar_message,
  weight_numeric_message
) {
  .network_input_validate_entry_lengths(
    entries,
    allowed_lengths,
    length_message
  )

  values <- lapply(
    seq_along(value_names),
    function(index) {
      vapply(
        entries,
        function(entry) {
          .network_input_scalar_numeric(
            entry,
            index,
            paste0(
              "Each ",
              row_name,
              " ",
              value_names[[index]],
              " value must be scalar."
            ),
            id_message
          )
        },
        numeric(1L)
      )
    }
  )
  weights <- vapply(
    entries,
    function(entry) {
      value <- if (length(entry) == length(value_names) + 1L) {
        entry[[length(entry)]]
      } else {
        1.0
      }
      .network_input_scalar_numeric(
        list(value),
        1L,
        weight_scalar_message,
        weight_numeric_message
      )
    },
    numeric(1L)
  )
  c(values, list(weights))
}

.network_input_multilayer_node_value <- function(node, name, index) {
  if (length(node) != 2L) {
    stop(
      "Each multilayer node must contain 2 values: layer id and node id.",
      call. = FALSE
    )
  }
  .network_input_scalar_numeric(
    node,
    index,
    paste0("Each multilayer ", name, " value must be scalar."),
    "Multilayer layer/node values must be numeric/integer."
  )
}

#' R6 class generator for the Infomap clustering algorithm
#'
#' @description
#' R6 class encapsulating an Infomap clustering session. Most users
#' should call the [Infomap()] wrapper rather than constructing this
#' class directly. The generator is exported for subclassing and
#' method introspection.
#'
#' @details
#' Methods are grouped here as input (build the network), run
#' (optimise the partition), igraph integration, results (read out
#' module assignments and node attributes), and writers (export to
#' .tree / .clu / etc).
#'
#' @rdname Infomap
#' @export
InfomapClass <- R6::R6Class(
  "Infomap",
  cloneable = FALSE,
  public = list(
    #' @description Create a new Infomap instance.
    #' @param args Optional raw CLI argument string to prepend.
    #' @param opts Optional options list from [infomap_options()].
    #' @param ... Named option overrides forwarded to [infomap_options()].
    initialize = function(args = NULL, opts = NULL, ...) {
      overrides <- list(...)
      if (is.null(opts)) {
        opts <- do.call(infomap_options, overrides)
      } else if (length(overrides) > 0L) {
        for (name in names(overrides)) {
          opts[[name]] <- overrides[[name]]
        }
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

    #' @description Read network data from file.
    #' @param filename Path to a network file (`.net`, `.txt`, `.tree`, etc.).
    #' @param accumulate If `TRUE` (default), accumulate to existing nodes/links.
    read_file = function(filename, accumulate = TRUE) {
      private$.swig$readInputData(
        as.character(filename),
        as.logical(accumulate)
      )
      invisible(self)
    },

    #' @description Add a node.
    #' @param node_id Integer node id.
    #' @param name Optional node name.
    #' @param teleportation_weight Optional teleportation weight.
    add_node = function(node_id, name = NULL, teleportation_weight = NULL) {
      id <- as.integer(node_id)
      if (!is.null(name) && !is.null(teleportation_weight)) {
        private$.swig$addNode(
          id,
          as.character(name),
          as.numeric(teleportation_weight)
        )
      } else if (!is.null(name)) {
        private$.swig$addNode(id, as.character(name))
      } else if (!is.null(teleportation_weight)) {
        private$.swig$addNode(id, as.numeric(teleportation_weight))
      } else {
        private$.swig$addNode(id)
      }
      invisible(self)
    },

    #' @description Add many nodes at once.
    #' @param nodes A list of integer node ids, or a list of vectors of the form
    #'   `c(node_id, name, teleportation_weight)` (last two optional), or a
    #'   named list/vector mapping `node_id` to `name` or to
    #'   `c(name, teleportation_weight)`.
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
            self$add_node(
              id,
              name = as.character(attr[[1]]),
              teleportation_weight = as.numeric(attr[[2]])
            )
          } else {
            self$add_node(id, name = as.character(attr))
          }
        }
      }
      invisible(self)
    },

    #' @description Add a state node.
    #' @param state_id Integer state node id.
    #' @param node_id Integer physical node id.
    add_state_node = function(state_id, node_id) {
      private$.swig$addStateNode(as.integer(state_id), as.integer(node_id))
      invisible(self)
    },

    #' @description Add many state nodes at once.
    #' @param state_nodes A list of `c(state_id, node_id)` vectors, or a
    #'   named list/vector mapping `state_id` to `node_id`.
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

    #' @description Set the name of a node.
    #' @param node_id Integer node id.
    #' @param name Node name (character). `NULL` clears the name.
    set_name = function(node_id, name) {
      if (is.null(name)) {
        name <- ""
      }
      private$.swig$addName(as.integer(node_id), as.character(name))
      invisible(self)
    },

    #' @description Set names for several nodes at once.
    #' @param mapping A list of `c(node_id, name)` vectors, or a named
    #'   list/vector mapping `node_id` to `name`.
    set_names = function(mapping) {
      if (is.null(names(mapping))) {
        for (entry in mapping) {
          self$set_name(entry[[1L]], entry[[2L]])
        }
      } else {
        for (id_str in names(mapping)) {
          self$set_name(as.integer(id_str), mapping[[id_str]])
        }
      }
      invisible(self)
    },

    #' @description Add a link.
    #' @param source_id Source node id.
    #' @param target_id Target node id.
    #' @param weight Link weight.
    add_link = function(source_id, target_id, weight = 1.0) {
      private$.swig$addLink(
        as.integer(source_id),
        as.integer(target_id),
        as.numeric(weight)
      )
      invisible(self)
    },

    #' @description Add many links at once.
    #' @param links A list of vectors of the form `c(source, target, weight)`
    #'   (weight optional), or a 2- or 3-column matrix / data.frame whose
    #'   first two columns are integer/numeric node ids and (optionally)
    #'   third column is numeric weight.
    add_links = function(links) {
      if (is.matrix(links) || is.data.frame(links)) {
        ncol_links <- ncol(links)
        if (!ncol_links %in% c(2L, 3L)) {
          stop(
            "`links` matrix/data.frame must have 2 or 3 columns ",
            "(source, target, [weight]).",
            call. = FALSE
          )
        }
        id_columns <- .network_input_columns(links, c(1L, 2L))
        .network_input_require_numeric(
          id_columns,
          "`links` source/target columns must be numeric/integer."
        )
        sources <- id_columns[[1L]]
        targets <- id_columns[[2L]]
        weights <- .network_input_weights(
          links,
          3L,
          length(sources),
          "`links` weight column must be numeric."
        )
      } else {
        link_lengths <- .network_input_validate_entry_lengths(
          links,
          c(2L, 3L),
          "Each link must contain 2 or 3 values: source, target, and optional weight."
        )
        sources <- vapply(
          links,
          function(entry) {
            .network_input_scalar_numeric(
              entry,
              1L,
              "Each link source value must be scalar.",
              "`links` source/target values must be numeric/integer."
            )
          },
          numeric(1L)
        )
        targets <- vapply(
          links,
          function(entry) {
            .network_input_scalar_numeric(
              entry,
              2L,
              "Each link target value must be scalar.",
              "`links` source/target values must be numeric/integer."
            )
          },
          numeric(1L)
        )
        weights <- vapply(
          links,
          function(entry) {
            value <- if (length(entry) == 3L) entry[[3L]] else 1.0
            .network_input_scalar_numeric(
              list(value),
              1L,
              "Each link weight value must be scalar.",
              "`links` weight values must be numeric."
            )
          },
          numeric(1L)
        )
      }

      private$.swig$addLinks(
        as.integer(sources),
        as.integer(targets),
        as.numeric(weights)
      )
      invisible(self)
    },

    #' @description Remove a link.
    #' @param source_id Source node id.
    #' @param target_id Target node id.
    remove_link = function(source_id, target_id) {
      private$.swig$network()$removeLink(
        as.integer(source_id),
        as.integer(target_id)
      )
      invisible(self)
    },

    #' @description Remove many links at once.
    #' @param links A list of `c(source, target)` vectors.
    remove_links = function(links) {
      for (entry in links) {
        self$remove_link(entry[[1L]], entry[[2L]])
      }
      invisible(self)
    },

    #' @description Add a multilayer link.
    #' @param source_multilayer_node A `c(layer_id, node_id)` vector or
    #'   the result of [multilayer_node()].
    #' @param target_multilayer_node A `c(layer_id, node_id)` vector or
    #'   the result of [multilayer_node()].
    #' @param weight Link weight.
    add_multilayer_link = function(
      source_multilayer_node,
      target_multilayer_node,
      weight = 1.0
    ) {
      src <- as.integer(source_multilayer_node)
      tgt <- as.integer(target_multilayer_node)
      private$.swig$addMultilayerLink(
        src[[1L]],
        src[[2L]],
        tgt[[1L]],
        tgt[[2L]],
        as.numeric(weight)
      )
      invisible(self)
    },

    #' @description Add an intra-layer link in a multilayer network.
    #' @param layer_id Integer layer id.
    #' @param source_node_id Source node id.
    #' @param target_node_id Target node id.
    #' @param weight Link weight.
    add_multilayer_intra_link = function(
      layer_id,
      source_node_id,
      target_node_id,
      weight = 1.0
    ) {
      private$.swig$addMultilayerIntraLink(
        as.integer(layer_id),
        as.integer(source_node_id),
        as.integer(target_node_id),
        as.numeric(weight)
      )
      invisible(self)
    },

    #' @description Add many intra-layer links in a multilayer network.
    #' @param links A list of vectors of the form
    #'   `c(layer, source_node, target_node, weight)` (weight optional), or
    #'   a 3- or 4-column matrix / data.frame.
    add_multilayer_intra_links = function(links) {
      if (is.matrix(links) || is.data.frame(links)) {
        parts <- .network_input_table(
          links,
          c(3L, 4L),
          "`links` matrix/data.frame must have 3 or 4 columns (layer, source_node, target_node, [weight]).",
          c(1L, 2L, 3L),
          "`links` multilayer intra-link id columns must be numeric/integer.",
          4L,
          "`links` weight column must be numeric."
        )
        layers <- parts[[1L]]
        sources <- parts[[2L]]
        targets <- parts[[3L]]
        weights <- parts[[4L]]
      } else {
        parts <- .network_input_flat_rows(
          links,
          c(3L, 4L),
          "Each multilayer intra-link must contain 3 or 4 values: layer, source node, target node, and optional weight.",
          c("layer", "source node", "target node"),
          "multilayer intra-link",
          "Multilayer intra-link id values must be numeric/integer.",
          "Each multilayer intra-link weight value must be scalar.",
          "Multilayer intra-link weight values must be numeric."
        )
        layers <- parts[[1L]]
        sources <- parts[[2L]]
        targets <- parts[[3L]]
        weights <- parts[[4L]]
      }

      private$.swig$addMultilayerIntraLinks(
        as.integer(layers),
        as.integer(sources),
        as.integer(targets),
        as.numeric(weights)
      )
      invisible(self)
    },

    #' @description Add an inter-layer link in a multilayer network.
    #' @param source_layer_id Source layer id.
    #' @param node_id Physical node id (same in both layers).
    #' @param target_layer_id Target layer id.
    #' @param weight Link weight.
    add_multilayer_inter_link = function(
      source_layer_id,
      node_id,
      target_layer_id,
      weight = 1.0
    ) {
      private$.swig$addMultilayerInterLink(
        as.integer(source_layer_id),
        as.integer(node_id),
        as.integer(target_layer_id),
        as.numeric(weight)
      )
      invisible(self)
    },

    #' @description Add many inter-layer links in a multilayer network.
    #' @param links A list of vectors of the form
    #'   `c(source_layer, node, target_layer, weight)` (weight optional), or
    #'   a 3- or 4-column matrix / data.frame.
    add_multilayer_inter_links = function(links) {
      if (is.matrix(links) || is.data.frame(links)) {
        parts <- .network_input_table(
          links,
          c(3L, 4L),
          "`links` matrix/data.frame must have 3 or 4 columns (source_layer, node, target_layer, [weight]).",
          c(1L, 2L, 3L),
          "`links` multilayer inter-link id columns must be numeric/integer.",
          4L,
          "`links` weight column must be numeric."
        )
        source_layers <- parts[[1L]]
        nodes <- parts[[2L]]
        target_layers <- parts[[3L]]
        weights <- parts[[4L]]
      } else {
        parts <- .network_input_flat_rows(
          links,
          c(3L, 4L),
          "Each multilayer inter-link must contain 3 or 4 values: source layer, node, target layer, and optional weight.",
          c("source layer", "node", "target layer"),
          "multilayer inter-link",
          "Multilayer inter-link id values must be numeric/integer.",
          "Each multilayer inter-link weight value must be scalar.",
          "Multilayer inter-link weight values must be numeric."
        )
        source_layers <- parts[[1L]]
        nodes <- parts[[2L]]
        target_layers <- parts[[3L]]
        weights <- parts[[4L]]
      }

      private$.swig$addMultilayerInterLinks(
        as.integer(source_layers),
        as.integer(nodes),
        as.integer(target_layers),
        as.numeric(weights)
      )
      invisible(self)
    },

    #' @description Add many multilayer links at once.
    #' @param links A list whose entries are
    #'   `list(source_multilayer_node, target_multilayer_node, weight)`
    #'   (weight optional), or a 4- or 5-column matrix / data.frame of
    #'   `source_layer`, `source_node`, `target_layer`, `target_node`, and
    #'   optional `weight`.
    add_multilayer_links = function(links) {
      if (is.matrix(links) || is.data.frame(links)) {
        parts <- .network_input_table(
          links,
          c(4L, 5L),
          "`links` matrix/data.frame must have 4 or 5 columns (source_layer, source_node, target_layer, target_node, [weight]).",
          c(1L, 2L, 3L, 4L),
          "`links` multilayer id columns must be numeric/integer.",
          5L,
          "`links` weight column must be numeric."
        )
        source_layers <- parts[[1L]]
        source_nodes <- parts[[2L]]
        target_layers <- parts[[3L]]
        target_nodes <- parts[[4L]]
        weights <- parts[[5L]]
      } else {
        .network_input_validate_entry_lengths(
          links,
          c(2L, 3L),
          "Each multilayer link must contain 2 or 3 values: source node, target node, and optional weight."
        )

        source_layers <- vapply(
          links,
          function(entry) {
            .network_input_multilayer_node_value(
              entry[[1L]],
              "source layer",
              1L
            )
          },
          numeric(1L)
        )
        source_nodes <- vapply(
          links,
          function(entry) {
            .network_input_multilayer_node_value(entry[[1L]], "source node", 2L)
          },
          numeric(1L)
        )
        target_layers <- vapply(
          links,
          function(entry) {
            .network_input_multilayer_node_value(
              entry[[2L]],
              "target layer",
              1L
            )
          },
          numeric(1L)
        )
        target_nodes <- vapply(
          links,
          function(entry) {
            .network_input_multilayer_node_value(entry[[2L]], "target node", 2L)
          },
          numeric(1L)
        )
        weights <- vapply(
          links,
          function(entry) {
            value <- if (length(entry) == 3L) entry[[3L]] else 1.0
            .network_input_scalar_numeric(
              list(value),
              1L,
              "Each multilayer link weight value must be scalar.",
              "Multilayer link weight values must be numeric."
            )
          },
          numeric(1L)
        )
      }

      private$.swig$addMultilayerLinks(
        as.integer(source_layers),
        as.integer(source_nodes),
        as.integer(target_layers),
        as.integer(target_nodes),
        as.numeric(weights)
      )
      invisible(self)
    },

    #' @description Set meta data for a node.
    #' @param node_id Integer node id.
    #' @param meta_category Integer meta category.
    set_meta_data = function(node_id, meta_category) {
      private$.swig$network()$addMetaData(
        as.integer(node_id),
        as.integer(meta_category)
      )
      invisible(self)
    },

    # ----------------------------------------
    # Run
    # ----------------------------------------

    #' @description Run Infomap.
    #' @param args Optional raw CLI argument string for this run only.
    #' @param opts Optional [infomap_options()] result for this run only.
    #' @param ... Named option overrides for this run only.
    run = function(args = NULL, opts = NULL, ...) {
      overrides <- list(...)
      if (is.null(opts)) {
        opts <- do.call(infomap_options, overrides)
      } else if (length(overrides) > 0L) {
        for (name in names(overrides)) {
          opts[[name]] <- overrides[[name]]
        }
      }
      # Mirror Python: if add_igraph() saw a directed graph and the user
      # never set a flow model (at construction or here, in opts or args),
      # treat the network as directed.
      if (
        isTRUE(private$.directed_from_igraph) &&
          !private$.flow_model_set &&
          is.null(opts$flow_model) &&
          is.null(opts$directed) &&
          !.flow_model_in_args(args)
      ) {
        opts$directed <- TRUE
      }
      cli <- construct_args(args, opts)
      private$.swig$run(cli)
      invisible(self)
    },

    #' @description Get the bipartite start id (the node id where the second node type starts).
    get_bipartite_start_id = function() {
      private$.swig$network()$bipartiteStartId()
    },

    #' @description Set the bipartite start id.
    #' @param start_id Integer node id where the second node type starts.
    set_bipartite_start_id = function(start_id) {
      private$.swig$setBipartiteStartId(as.integer(start_id))
      invisible(self)
    },

    #' @description Print a short summary.
    #' @param ... Unused.
    print = function(...) {
      cat("<Infomap>\n")
      cat("  num_top_modules:", private$.swig$numTopModules(), "\n")
      cat("  codelength:", format(private$.swig$codelength(), digits = 6), "\n")
      invisible(self)
    },

    # ----------------------------------------
    # igraph integration
    # ----------------------------------------

    #' @description Import an igraph graph.
    #' @details
    #' Translated from `interfaces/python/src/infomap/_networkx.py`.
    #'
    #' **Node id convention.** Infomap result accessors report the numeric
    #' node ids used to build the network. For links added directly with
    #' `add_link()` or `add_links()`, those user-supplied ids are preserved.
    #' `add_igraph()` uses R igraph's 1-indexed vertex ids as state ids.
    #' Plain graph results therefore use those ids directly. State and
    #' multilayer graphs may use separate physical ids from `phys_id`; if
    #' those ids are labels, they are mapped to stable internal integers and
    #' returned as `attr(mapping, "phys_id")`. If the original igraph had
    #' vertex names (`V(g)$name`), the returned mapping recovers them.
    #'
    #' If the graph is directed, `run()` will inject `--directed`
    #' unless the user has already chosen a flow model via `opts` or
    #' raw args.
    #'
    #' @param g An igraph graph.
    #' @param weight Edge attribute to use as link weight. Use `NULL` to use
    #'   `"weight"` when present, or `FALSE` to ignore edge weights.
    #' @param phys_id Vertex attribute holding physical node ids
    #'   (state-node case).
    #' @param layer_id Vertex attribute holding layer ids
    #'   (multilayer case).
    #' @param multilayer_inter_intra_format If `TRUE`, intra/inter
    #'   format is used; diagonal links (different layer AND different
    #'   physical node) trigger an error. Set to `FALSE` to allow
    #'   `add_multilayer_link()` for arbitrary (layer, node) pairs.
    #' @return Invisibly returns a named character vector mapping igraph
    #'   vertex ids to the original igraph vertex names (or stringified
    #'   vertex ids when `V(g)$name` is absent). For state or multilayer
    #'   networks with non-numeric `phys_id` labels, the returned vector has
    #'   a `"phys_id"` attribute mapping internal physical ids to original
    #'   labels. Useful for joining Infomap results back to the original graph.
    add_igraph = function(
      g,
      weight = "weight",
      phys_id = "phys_id",
      layer_id = "layer_id",
      multilayer_inter_intra_format = TRUE
    ) {
      if (!requireNamespace("igraph", quietly = TRUE)) {
        stop(
          "The 'igraph' package is required for add_igraph(). ",
          "Install it with install.packages(\"igraph\").",
          call. = FALSE
        )
      }
      if (!inherits(g, "igraph")) {
        stop("`g` must be an igraph graph.", call. = FALSE)
      }

      is_directed <- igraph::is_directed(g)
      has_phys <- length(phys_id) == 1L &&
        phys_id %in% igraph::vertex_attr_names(g)
      has_layer <- length(layer_id) == 1L &&
        layer_id %in% igraph::vertex_attr_names(g)

      vertex_names <- igraph::V(g)$name
      if (is.null(vertex_names)) {
        vertex_names <- as.character(seq_len(igraph::vcount(g)))
      }

      vertex_ids <- seq_len(igraph::vcount(g))
      mapping <- stats::setNames(vertex_names, as.character(vertex_ids))

      if (has_phys) {
        phys_data <- .map_igraph_phys_ids(igraph::vertex_attr(g, phys_id))
        phys <- phys_data$ids
        if (!is.null(phys_data$mapping)) {
          attr(mapping, "phys_id") <- phys_data$mapping
        }
      } else {
        phys <- vertex_ids
      }

      if (!has_phys && is.character(vertex_names)) {
        for (i in seq_len(igraph::vcount(g))) {
          self$add_node(vertex_ids[i], name = vertex_names[i])
        }
      }

      # Vertex names are captured in `mapping` above; edge endpoints use
      # R igraph's 1-indexed vertex ids.
      edges <- igraph::as_edgelist(g, names = FALSE)
      weights <- .igraph_edge_weights(g, weight, nrow(edges))

      if (has_phys && has_layer) {
        # Multilayer state network.
        layers <- as.integer(igraph::vertex_attr(g, layer_id))
        for (i in seq_len(igraph::vcount(g))) {
          self$add_state_node(vertex_ids[i], phys[i])
        }

        if (isTRUE(multilayer_inter_intra_format)) {
          edge_sources <- edges[, 1L]
          edge_targets <- edges[, 2L]
          same_layer <- layers[edge_sources] == layers[edge_targets]
          diagonal <- !same_layer & phys[edge_sources] != phys[edge_targets]
          if (any(diagonal)) {
            stop(
              "Multilayer intra/inter format does not support diagonal links ",
              "(edge between different physical nodes in different layers). ",
              "Use `multilayer_inter_intra_format = FALSE`.",
              call. = FALSE
            )
          }

          if (any(same_layer)) {
            intra_sources <- edge_sources[same_layer]
            intra_targets <- edge_targets[same_layer]
            self$add_multilayer_intra_links(
              cbind(
                layers[intra_sources],
                phys[intra_sources],
                phys[intra_targets],
                weights[same_layer]
              )
            )
          }
          if (any(!same_layer)) {
            inter_sources <- edge_sources[!same_layer]
            inter_targets <- edge_targets[!same_layer]
            self$add_multilayer_inter_links(
              cbind(
                layers[inter_sources],
                phys[inter_sources],
                layers[inter_targets],
                weights[!same_layer]
              )
            )
          }
        } else {
          for (e in seq_len(nrow(edges))) {
            s <- edges[e, 1L]
            t <- edges[e, 2L]
            self$add_multilayer_link(
              c(layers[s], phys[s]),
              c(layers[t], phys[t]),
              weights[e]
            )
          }
        }
      } else if (has_phys) {
        # State network without explicit layer info.
        for (i in seq_len(igraph::vcount(g))) {
          self$add_state_node(vertex_ids[i], phys[i])
        }
        self$add_links(cbind(edges[, 1L], edges[, 2L], weights))
      } else {
        # Plain network.
        self$add_links(cbind(edges[, 1L], edges[, 2L], weights))
      }

      if (is_directed) {
        # Mirror Python: remember that the source graph is directed.
        # run() injects --directed unless the user has set a flow model
        # (via opts at construction/run, or via raw args).
        private$.directed_from_igraph <- TRUE
      }
      private$.igraph_vcount <- igraph::vcount(g)
      private$.igraph_have_memory <- has_phys

      invisible(mapping)
    },

    #' @description Convert the Infomap partition to an igraph
    #'   `communities` object (from `igraph::make_clusters()`),
    #'   compatible with `modularity()`, `membership()`, `plot()`.
    #' @param g The same igraph graph passed to `add_igraph()`.
    as_communities = function(g) {
      if (!requireNamespace("igraph", quietly = TRUE)) {
        stop(
          "The 'igraph' package is required for as_communities(). ",
          "Install it with install.packages(\"igraph\").",
          call. = FALSE
        )
      }
      if (!inherits(g, "igraph")) {
        stop("`g` must be an igraph graph.", call. = FALSE)
      }
      if (
        !is.null(private$.igraph_vcount) &&
          igraph::vcount(g) != private$.igraph_vcount
      ) {
        stop(
          "`g` must be the same igraph graph passed to add_igraph().",
          call. = FALSE
        )
      }

      # add_igraph() uses R igraph's 1-indexed vertex ids as state ids. For
      # state/multilayer graphs, membership therefore has to be looked up by
      # state id; plain graphs can keep using physical node ids.
      mods <- if (isTRUE(private$.igraph_have_memory)) {
        self$get_modules(states = TRUE)
      } else {
        self$modules
      }
      n <- igraph::vcount(g)
      vertex_ids <- as.character(seq_len(n))
      membership <- as.integer(mods[vertex_ids])
      if (length(membership) != n || anyNA(membership)) {
        stop(
          "Could not align Infomap membership with `g`; `g` must be the ",
          "same igraph graph passed to add_igraph().",
          call. = FALSE
        )
      }
      igraph::make_clusters(
        g,
        membership = membership,
        algorithm = "Infomap",
        modularity = FALSE
      )
    },

    # ----------------------------------------
    # Results
    # ----------------------------------------
    # SWIG R does not auto-convert every STL type to native R types, so
    # result accessors walk iterator/vector wrappers and accumulate values
    # into R vectors.

    #' @description Get module assignment per leaf node.
    #' @param depth_level Tree depth used for the module id. `1` gives
    #'   top-level modules, `-1` the bottom level.
    #' @param states If `TRUE`, return one entry per state node (for
    #'   higher-order networks); otherwise one per physical node.
    #' @return A named integer vector mapping node id (or state id) to
    #'   module id.
    get_modules = function(depth_level = 1L, states = FALSE) {
      it <- if (self$have_memory && !isTRUE(states)) {
        private$.swig$iterTreePhysical(as.integer(depth_level))
      } else {
        private$.swig$iterLeafNodes(as.integer(depth_level))
      }

      n_max <- self$num_leaf_nodes
      ids <- integer(n_max)
      modules <- integer(n_max)
      i <- 0L
      while (!it$isEnd()) {
        if (it$isLeaf()) {
          i <- i + 1L
          ids[i] <- as.integer(
            if (isTRUE(states)) it$stateId else it$physicalId
          )
          modules[i] <- as.integer(it$moduleId())
        }
        it$stepForward()
      }
      ids <- ids[seq_len(i)]
      modules <- modules[seq_len(i)]
      names(modules) <- as.character(ids)
      modules
    },

    #' @description Get the full module path for each leaf node.
    #' @param states If `TRUE`, use state ids for higher-order networks.
    #' @return A named list mapping node id (or state id) to an integer
    #'   vector of module ids per level.
    get_multilevel_modules = function(states = FALSE) {
      depth <- self$num_levels
      if (depth < 1L) {
        return(structure(list(), names = character()))
      }
      per_level <- lapply(seq_len(depth), function(d) {
        self$get_modules(d, states)
      })
      ids <- names(per_level[[1L]])
      out <- lapply(ids, function(id) {
        vapply(per_level, function(m) m[[id]], integer(1L))
      })
      names(out) <- ids
      out
    },

    #' @description Get per-leaf-node attributes (state id, physical id,
    #'   module id, flow, optional layer id).
    #' @param depth_level Tree depth used for the module id.
    #' @param states If `TRUE`, return one row per state node.
    #' @return A list of integer/numeric vectors.
    get_nodes = function(depth_level = 1L, states = FALSE) {
      it <- if (self$have_memory && !isTRUE(states)) {
        private$.swig$iterTreePhysical(as.integer(depth_level))
      } else {
        private$.swig$iterLeafNodes(as.integer(depth_level))
      }

      n_max <- self$num_leaf_nodes
      state_id <- integer(n_max)
      node_id <- integer(n_max)
      module_id <- integer(n_max)
      flow <- numeric(n_max)
      layer_id <- integer(n_max)
      has_layer <- self$have_memory
      i <- 0L
      while (!it$isEnd()) {
        if (it$isLeaf()) {
          i <- i + 1L
          state_id[i] <- as.integer(it$stateId)
          node_id[i] <- as.integer(it$physicalId)
          module_id[i] <- as.integer(it$moduleId())
          flow[i] <- as.numeric(it$data$flow)
          if (has_layer) layer_id[i] <- as.integer(it$layerId)
        }
        it$stepForward()
      }
      out <- list(
        state_id = state_id[seq_len(i)],
        node_id = node_id[seq_len(i)],
        module_id = module_id[seq_len(i)],
        flow = flow[seq_len(i)]
      )
      if (has_layer) {
        out$layer_id <- layer_id[seq_len(i)]
      }
      out
    },

    #' @description Get per-link weights and flow.
    #' @details
    #' For ordinary networks added with `add_link()` or `add_links()`,
    #' `source` and `target` are the user-supplied node ids. For state
    #' and multilayer networks they are state ids. Before `run()`,
    #' `weight` reflects the input weights and `flow` reflects the core
    #' link-flow values currently stored by Infomap, usually zero.
    #' @return A `data.frame` with columns `source`, `target`, `weight`,
    #'   and `flow`.
    get_links = function() {
      raw <- private$.swig$getLinkResults()
      n <- as.integer(raw$size())

      source <- integer(n)
      target <- integer(n)
      weight <- numeric(n)
      flow <- numeric(n)

      for (i in seq_len(n)) {
        link <- raw$`__getitem__`(as.integer(i - 1L))
        source[i] <- as.integer(link$source)
        target[i] <- as.integer(link$target)
        weight[i] <- as.numeric(link$weight)
        flow[i] <- as.numeric(link$flow)
      }

      data.frame(
        source = source,
        target = target,
        weight = weight,
        flow = flow
      )
    },

    #' @description Look up a node's name.
    #' @param node_id Integer node id.
    #' @param default Value returned when the node has no name.
    #' @return Character (or `default`).
    get_name = function(node_id, default = NULL) {
      name <- private$.swig$getName(as.integer(node_id))
      if (identical(name, "")) {
        return(default)
      }
      name
    },

    #' @description Get all assigned node names.
    #' @return A named character vector mapping node id to name.
    get_names = function() {
      # SWIG R returns std::map as an opaque pointer, so walk leaf nodes
      # and ask for each name individually.
      ids <- self$get_nodes(depth_level = 1L, states = TRUE)$node_id
      ids <- unique(ids)
      out <- vapply(
        ids,
        function(id) {
          name <- private$.swig$getName(as.integer(id))
          if (is.null(name)) "" else name
        },
        character(1L)
      )
      names(out) <- as.character(ids)
      out[nzchar(out)]
    },

    #' @description Look up a state node's name.
    #' @param state_id Integer state node id.
    #' @param default Value returned when the state node has no name.
    #' @return Character (or `default`).
    get_state_name = function(state_id, default = NULL) {
      name <- private$.swig$getStateName(as.integer(state_id))
      if (identical(name, "")) {
        return(default)
      }
      name
    },

    #' @description Get all assigned state node names.
    #' @details
    #' Populated for higher-order (state/memory) networks whose `*States`
    #' section names the state nodes; empty otherwise. Physical node names
    #' are available separately via `get_names()`.
    #' @return A named character vector mapping state id to name.
    get_state_names = function() {
      # SWIG R returns std::map as an opaque pointer, so walk state nodes
      # and ask for each name individually.
      ids <- self$get_nodes(depth_level = 1L, states = TRUE)$state_id
      ids <- unique(ids)
      out <- vapply(
        ids,
        function(id) {
          name <- private$.swig$getStateName(as.integer(id))
          if (is.null(name)) "" else name
        },
        character(1L)
      )
      names(out) <- as.character(ids)
      out[nzchar(out)]
    },

    # ----------------------------------------
    # Writers
    # ----------------------------------------

    #' @description Write the partition as a `.clu` file.
    #' @param filename Output path.
    #' @param states Whether to write state ids (default `FALSE`).
    #' @param depth_level Tree depth used for the module id.
    write_clu = function(filename, states = FALSE, depth_level = 1L) {
      private$.swig$writeClu(
        as.character(filename),
        as.logical(states),
        as.integer(depth_level)
      )
      invisible(filename)
    },

    #' @description Write the partition as a `.tree` file.
    #' @param filename Output path.
    #' @param states Whether to include state ids.
    write_tree = function(filename, states = FALSE) {
      private$.swig$writeTree(as.character(filename), as.logical(states))
      invisible(filename)
    },

    #' @description Write the partition as a `.ftree` (flow-tree) file.
    #' @param filename Output path.
    #' @param states Whether to include state ids.
    write_flow_tree = function(filename, states = FALSE) {
      private$.swig$writeFlowTree(as.character(filename), as.logical(states))
      invisible(filename)
    },

    #' @description Write the partition as a Newick `.nwk` file.
    #' @param filename Output path.
    #' @param states Whether to include state ids.
    write_newick = function(filename, states = FALSE) {
      private$.swig$writeNewickTree(as.character(filename), as.logical(states))
      invisible(filename)
    },

    #' @description Write the partition as a JSON file.
    #' @param filename Output path.
    #' @param states Whether to include state ids.
    write_json = function(filename, states = FALSE) {
      private$.swig$writeJsonTree(as.character(filename), as.logical(states))
      invisible(filename)
    },

    #' @description Write the partition as a CSV file.
    #' @param filename Output path.
    #' @param states Whether to include state ids.
    write_csv = function(filename, states = FALSE) {
      private$.swig$writeCsvTree(as.character(filename), as.logical(states))
      invisible(filename)
    },

    #' @description Write the input network in Pajek `.net` format.
    #' @param filename Output path.
    #' @param flow Whether to write computed flow values per link.
    write_pajek = function(filename, flow = FALSE) {
      private$.swig$network()$writePajekNetwork(
        as.character(filename),
        as.logical(flow)
      )
      invisible(filename)
    },

    #' @description Write the state network.
    #' @param filename Output path.
    write_state_network = function(filename) {
      private$.swig$network()$writeStateNetwork(as.character(filename))
      invisible(filename)
    },

    #' @description Dispatch on file extension to the appropriate
    #'   writer (`.tree`, `.ftree`, `.nwk`, `.clu`, `.json`, `.csv`,
    #'   `.net`).
    #' @param filename Output path. Extension chooses the writer.
    #' @param ... Forwarded to the chosen writer.
    write = function(filename, ...) {
      ext <- tolower(tools::file_ext(filename))
      ext <- switch(
        ext,
        "ftree" = "flow_tree",
        "nwk" = "newick",
        "net" = "pajek",
        ext
      )
      method_name <- paste0("write_", ext)
      if (!is.function(self[[method_name]])) {
        stop("No method found for writing ", ext, " files", call. = FALSE)
      }
      self[[method_name]](filename, ...)
    }
  ),
  active = list(
    #' @field swig The underlying SWIG-generated InfomapWrapper handle.
    swig = function() private$.swig,
    #' @field network The underlying Network reference.
    network = function() private$.swig$network(),
    #' @field codelength Total (hierarchical) codelength.
    codelength = function() private$.swig$codelength(),
    #' @field codelengths Codelength of each trial.
    codelengths = function() {
      v <- private$.swig$codelengths()
      n <- v$size()
      if (n == 0L) {
        return(numeric(0L))
      }
      vapply(
        seq_len(n) - 1L,
        function(i) v$"__getitem__"(as.integer(i)),
        numeric(1L)
      )
    },
    #' @field num_top_modules Number of top modules in the tree.
    num_top_modules = function() private$.swig$numTopModules(),
    #' @field num_non_trivial_top_modules Number of non-trivial top modules.
    num_non_trivial_top_modules = function() {
      private$.swig$numNonTrivialTopModules()
    },
    #' @field num_levels Number of levels in the tree.
    num_levels = function() private$.swig$numLevels(),
    #' @field max_tree_depth Maximum depth of the tree.
    max_tree_depth = function() private$.swig$maxTreeDepth(),
    #' @field num_leaf_nodes Number of leaf nodes in the tree.
    num_leaf_nodes = function() private$.swig$numLeafNodes(),
    #' @field num_nodes Number of nodes (state nodes for higher-order networks).
    num_nodes = function() private$.swig$network()$numNodes(),
    #' @field num_physical_nodes Number of physical nodes.
    num_physical_nodes = function() private$.swig$network()$numPhysicalNodes(),
    #' @field num_links Number of links.
    num_links = function() private$.swig$network()$numLinks(),
    #' @field have_memory `TRUE` if this is a state/multilayer network.
    have_memory = function() private$.swig$network()$haveMemoryInput(),
    #' @field index_codelength Index codelength (top-level part of the codelength).
    index_codelength = function() private$.swig$getIndexCodelength(),
    #' @field module_codelength Module codelength (within-module part).
    module_codelength = function() private$.swig$getModuleCodelength(),
    #' @field hierarchical_codelength Hierarchical codelength.
    hierarchical_codelength = function() {
      private$.swig$getHierarchicalCodelength()
    },
    #' @field one_level_codelength One-level codelength baseline.
    one_level_codelength = function() private$.swig$getOneLevelCodelength(),
    #' @field relative_codelength_savings Relative codelength savings.
    relative_codelength_savings = function() {
      private$.swig$getRelativeCodelengthSavings()
    },
    #' @field entropy_rate Entropy rate of the network.
    entropy_rate = function() private$.swig$getEntropyRate(),
    #' @field max_entropy Maximum possible entropy.
    max_entropy = function() private$.swig$getMaxEntropy(),
    #' @field bipartite_start_id Get or set the bipartite start id.
    bipartite_start_id = function(value) {
      if (missing(value)) {
        self$get_bipartite_start_id()
      } else {
        self$set_bipartite_start_id(value)
      }
    },
    #' @field modules Top-level module assignment per node (named integer vector).
    modules = function() {
      self$get_modules(depth_level = 1L, states = FALSE)
    },
    #' @field multilevel_modules List of integer paths for each node
    #'   through the module hierarchy.
    multilevel_modules = function() {
      self$get_multilevel_modules(states = FALSE)
    },
    #' @field nodes Per-state-node attributes (state id, physical id,
    #'   module, flow, optional layer id).
    nodes = function() {
      self$get_nodes(depth_level = 1L, states = TRUE)
    },
    #' @field physical_nodes Per-physical-node attributes (state nodes
    #'   merged within a module).
    physical_nodes = function() {
      self$get_nodes(depth_level = 1L, states = FALSE)
    },
    #' @field links Per-link source, target, weight and flow as a
    #'   `data.frame`.
    links = function() {
      self$get_links()
    },
    #' @field flow_links Per-link source, target and flow as a
    #'   `data.frame`.
    flow_links = function() {
      links <- self$get_links()
      links[c("source", "target", "flow")]
    },
    #' @field names Named character vector of all assigned node names.
    names = function() {
      self$get_names()
    }
  ),
  private = list(
    .swig = NULL,
    .flow_model_set = FALSE,
    .directed_from_igraph = FALSE,
    .igraph_vcount = NULL,
    .igraph_have_memory = FALSE
  )
)

# Heuristic: detect whether a raw CLI string already specifies a flow
# model. Imperfect parser (e.g. "--cluster-data --directed" matches
# greedily on -d), but covers the relevant flags. Used to avoid
# overriding user-supplied flags when add_igraph() sees a directed graph.
.flow_model_in_args <- function(args) {
  if (is.null(args) || !nzchar(args)) {
    return(FALSE)
  }
  grepl("(^|\\s)(--directed|-d\\b|--flow-model|-f\\b)", args)
}

.igraph_edge_weights <- function(g, weight, num_edges) {
  if (identical(weight, FALSE)) {
    return(rep(1.0, num_edges))
  }

  attr_name <- if (is.null(weight)) {
    "weight"
  } else {
    if (!is.character(weight) || length(weight) != 1L || is.na(weight)) {
      stop(
        "`weight` must be NULL, FALSE, or a single igraph edge attribute name.",
        call. = FALSE
      )
    }
    weight
  }

  if (!(attr_name %in% igraph::edge_attr_names(g))) {
    return(rep(1.0, num_edges))
  }

  weights <- igraph::edge_attr(g, attr_name)
  if (!is.numeric(weights)) {
    stop("`weight` edge attribute must be numeric.", call. = FALSE)
  }
  if (anyNA(weights)) {
    stop(
      "`weight` edge attribute cannot contain missing values.",
      call. = FALSE
    )
  }
  as.numeric(weights)
}

.map_igraph_phys_ids <- function(values) {
  if (anyNA(values)) {
    stop(
      "`phys_id` vertex attribute cannot contain missing values.",
      call. = FALSE
    )
  }

  if (is.numeric(values)) {
    ids <- as.integer(values)
    if (any(!is.finite(values)) || any(values != ids)) {
      stop(
        "`phys_id` vertex attribute must contain integer-like numeric values or non-missing labels.",
        call. = FALSE
      )
    }
    return(list(ids = ids, mapping = NULL))
  }

  labels <- as.character(values)
  if (anyNA(labels)) {
    stop(
      "`phys_id` vertex attribute cannot contain missing values.",
      call. = FALSE
    )
  }

  unique_labels <- unique(labels)
  ids <- match(labels, unique_labels)
  mapping <- stats::setNames(
    unique_labels,
    as.character(seq_along(unique_labels))
  )
  list(ids = as.integer(ids), mapping = mapping)
}
