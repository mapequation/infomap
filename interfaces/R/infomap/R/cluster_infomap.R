#' Cluster a network with Infomap
#'
#' @description
#' High-level helper for the common case: pass an edge list, run Infomap,
#' and get a compact result object with node assignments and summary fields.
#'
#' @details
#' For `data.frame` and `matrix` inputs, the first two columns are treated
#' as source and target node ids. Node ids must be numeric or integer and
#' are passed through to the low-level [Infomap()] API unchanged. If
#' `weight = NULL`, a third edge-list column is used as weight when present;
#' otherwise all links get weight `1`.
#'
#' For `igraph` inputs, this function delegates to `Infomap$add_igraph()`.
#' Infomap results use R igraph's 1-indexed vertex ids; the `mapping` field
#' maps those ids back to vertex names when the graph has names.
#'
#' @param edges A data frame, matrix, or igraph graph.
#' @param weight Edge weight column. For data frames, use a column name or
#'   numeric column index. For matrices, use a numeric column index. Set
#'   `FALSE` to ignore weights. For igraph graphs, use the edge attribute
#'   name, or `NULL` to use `"weight"` when present.
#' @param e.weights Alias for `weight`, provided for familiarity with
#'   `igraph::cluster_infomap()`. For igraph inputs, this can be an edge
#'   attribute name or a numeric vector with one value per edge. Do not
#'   pass both `weight` and `e.weights`.
#' @param v.weights Vertex weights are not supported by this high-level
#'   helper yet.
#' @param nb.trials Alias for the Infomap `num_trials` option, provided for
#'   familiarity with `igraph::cluster_infomap()`. Do not pass both
#'   `nb.trials` and `num_trials`.
#' @param args Optional raw CLI argument string passed to [Infomap()].
#' @param opts Optional options list from [infomap_options()].
#' @param tibble If `TRUE`, return `nodes` as a tibble (requires the
#'   `tibble` package). Default `FALSE` returns a plain data frame.
#' @param ... Named option overrides forwarded to [Infomap()].
#'
#' @return An object of class `"infomap_result"` with fields `nodes`,
#'   `modules`, `codelength`, `num_top_modules`, `model`, and related
#'   summary fields.
#'
#' @examples
#' edges <- data.frame(
#'   source = c(1, 1, 2, 3, 4, 4, 5),
#'   target = c(2, 3, 3, 4, 5, 6, 6)
#' )
#' result <- cluster_infomap(edges, silent = TRUE, num_trials = 5)
#' result$codelength
#' result$modules
#' as.data.frame(result)
#' @export
cluster_infomap <- function(edges,
                            weight = NULL,
                            e.weights = NULL,
                            v.weights = NULL,
                            nb.trials = NULL,
                            args = NULL,
                            opts = NULL,
                            tibble = FALSE,
                            ...) {
  call_options <- list(...)
  if (!is.null(e.weights)) {
    if (!is.null(weight)) {
      stop("Pass only one of `weight` and `e.weights`.", call. = FALSE)
    }
    weight <- e.weights
  }
  if (!is.null(v.weights)) {
    stop(
      "`v.weights` is not supported by cluster_infomap() yet. ",
      "Use the low-level Infomap API to set node weights.",
      call. = FALSE
    )
  }
  if (!is.null(nb.trials)) {
    if (!is.null(call_options$num_trials)) {
      stop("Pass only one of `num_trials` and `nb.trials`.", call. = FALSE)
    }
    call_options$num_trials <- nb.trials
  }

  im <- do.call(Infomap, c(list(args = args, opts = opts), call_options))
  mapping <- .add_cluster_input(im, edges, weight)
  im$run()

  nodes <- as.data.frame(im, tibble = tibble)
  out <- list(
    nodes = nodes,
    modules = im$modules,
    codelength = im$codelength,
    index_codelength = im$index_codelength,
    module_codelength = im$module_codelength,
    one_level_codelength = im$one_level_codelength,
    relative_codelength_savings = im$relative_codelength_savings,
    num_top_modules = im$num_top_modules,
    num_nodes = im$num_nodes,
    num_links = im$num_links,
    num_levels = im$num_levels,
    mapping = mapping,
    model = im
  )
  class(out) <- "infomap_result"
  out
}

#' @param x An `infomap_result` object.
#' @rdname cluster_infomap
#' @method print infomap_result
#' @export
print.infomap_result <- function(x, ...) {
  cat("<infomap_result>\n")
  cat("  nodes:", x$num_nodes, "\n")
  cat("  links:", x$num_links, "\n")
  cat("  modules:", x$num_top_modules, "\n")
  cat("  codelength:", format(x$codelength, digits = 6), "\n")
  invisible(x)
}

#' @param object An `infomap_result` object.
#' @rdname cluster_infomap
#' @method summary infomap_result
#' @export
summary.infomap_result <- function(object, ...) {
  out <- data.frame(
    num_nodes = object$num_nodes,
    num_links = object$num_links,
    num_top_modules = object$num_top_modules,
    num_levels = object$num_levels,
    codelength = object$codelength,
    one_level_codelength = object$one_level_codelength,
    relative_codelength_savings = object$relative_codelength_savings,
    stringsAsFactors = FALSE
  )
  class(out) <- c("summary.infomap_result", class(out))
  out
}

#' @param row.names Standard `as.data.frame` argument; ignored.
#' @param optional Standard `as.data.frame` argument; ignored.
#' @rdname cluster_infomap
#' @method as.data.frame infomap_result
#' @export
as.data.frame.infomap_result <- function(x,
                                         row.names = NULL,
                                         optional = FALSE,
                                         ...) {
  x$nodes
}

#' Cluster a multilayer network with Infomap
#'
#' @description
#' High-level helper for multilayer networks: pass a single data frame
#' (or matrix / tibble) of multilayer edges, run Infomap, and get an
#' `infomap_result` whose `nodes` data frame includes a `layer_id`
#' column.
#'
#' @details
#' Two input formats are recognised by column name (case-sensitive):
#'
#' * **Full multilayer**: columns `layer_from`, `node_from`, `layer_to`,
#'   `node_to`, plus an optional weight column. Each row is an arbitrary
#'   `(layer, node) -> (layer, node)` link, routed via
#'   `Infomap$add_multilayer_links()`. Use this format to mix
#'   intra-layer links (same `layer_from`/`layer_to`) and inter-layer
#'   links (same `node_from`/`node_to` across different layers) in a
#'   single edge list.
#' * **Intra-layer only**: columns `layer`, `node_from`, `node_to`, plus
#'   an optional weight column. Every row is a link within one layer,
#'   routed via `Infomap$add_multilayer_intra_links()`. With no explicit
#'   inter-layer links, Infomap couples layers via
#'   `multilayer_relax_rate` (default 0.15).
#'
#' Columns may appear in any order. Matrix inputs are interpreted
#' positionally in the orders listed above.
#'
#' For more exotic setups (e.g. weighted inter-layer couplings via
#' `add_multilayer_inter_links()`), use the lower-level [Infomap()] R6
#' API directly.
#'
#' @param multilayer_edges A data frame, tibble, or matrix of multilayer
#'   edges in one of the formats above.
#' @param weight Edge weight column name or numeric column index. Set
#'   `FALSE` to ignore weights. If `NULL` (default), uses a column named
#'   `weight` when present, then falls back to a trailing extra column,
#'   then to weight `1`.
#' @param args Optional raw CLI argument string passed to [Infomap()].
#' @param opts Optional options list from [infomap_options()].
#' @param tibble If `TRUE`, return `nodes` as a tibble (requires the
#'   `tibble` package). Default `FALSE` returns a plain data frame.
#' @param ... Named option overrides forwarded to [Infomap()].
#'
#' @return An object of class `"infomap_result"`. The `nodes` field
#'   includes a `layer_id` column.
#'
#' @examples
#' # Intra-layer only: two triangles, one per layer.
#' intra <- data.frame(
#'   layer     = c(1, 1, 1, 2, 2, 2),
#'   node_from = c(1, 2, 3, 1, 2, 3),
#'   node_to   = c(2, 3, 1, 2, 3, 1)
#' )
#' result <- cluster_infomap_multilayer(intra, silent = TRUE, num_trials = 3)
#' result$codelength
#' as.data.frame(result)
#'
#' # Full multilayer: intra-layer triangles + inter-layer couplings.
#' edges <- data.frame(
#'   layer_from = c(1, 1, 1, 2, 2, 2, 1, 1, 1),
#'   node_from  = c(1, 2, 3, 1, 2, 3, 1, 2, 3),
#'   layer_to   = c(1, 1, 1, 2, 2, 2, 2, 2, 2),
#'   node_to    = c(2, 3, 1, 2, 3, 1, 1, 2, 3),
#'   weight     = c(1, 1, 1, 1, 1, 1, 0.5, 0.5, 0.5)
#' )
#' result <- cluster_infomap_multilayer(edges, silent = TRUE, num_trials = 3)
#' result$num_top_modules
#' @export
cluster_infomap_multilayer <- function(multilayer_edges,
                                       weight = NULL,
                                       args = NULL,
                                       opts = NULL,
                                       tibble = FALSE,
                                       ...) {
  call_options <- list(...)
  im <- do.call(Infomap, c(list(args = args, opts = opts), call_options))
  .add_multilayer_input(im, multilayer_edges, weight)
  im$run()

  nodes <- as.data.frame(im, tibble = tibble)
  out <- list(
    nodes = nodes,
    modules = im$modules,
    codelength = im$codelength,
    index_codelength = im$index_codelength,
    module_codelength = im$module_codelength,
    one_level_codelength = im$one_level_codelength,
    relative_codelength_savings = im$relative_codelength_savings,
    num_top_modules = im$num_top_modules,
    num_nodes = im$num_nodes,
    num_links = im$num_links,
    num_levels = im$num_levels,
    mapping = NULL,
    model = im
  )
  class(out) <- "infomap_result"
  out
}

#' Convert an Infomap result to igraph communities
#'
#' @description
#' Converts an `infomap_result` created from an igraph graph to an igraph
#' `communities` object.
#'
#' @param x An `infomap_result` object.
#' @param graph The same igraph graph passed to [cluster_infomap()].
#' @param ... Unused.
#'
#' @return An igraph `communities` object.
#' @examples
#' if (requireNamespace("igraph", quietly = TRUE)) {
#'   graph <- igraph::make_graph(c(1, 2, 2, 3, 3, 1), directed = FALSE)
#'   result <- cluster_infomap(graph, silent = TRUE)
#'   as_communities(result, graph)
#' }
#' @export
as_communities <- function(x, graph, ...) {
  UseMethod("as_communities")
}

#' @rdname as_communities
#' @method as_communities infomap_result
#' @export
as_communities.infomap_result <- function(x, graph, ...) {
  x$model$as_communities(graph)
}

.add_cluster_input <- function(im, edges, weight) {
  if (inherits(edges, "igraph")) {
    imported <- .import_igraph_input(im, edges, weight)
    return(imported)
  }

  normalized <- .normalize_edge_list(edges, weight)
  im$add_links(normalized)
  NULL
}

.import_igraph_input <- function(im, graph, weight) {
  igraph_weight <- if (is.null(weight)) "weight" else weight
  if (is.numeric(igraph_weight) && length(igraph_weight) > 1L) {
    if (!requireNamespace("igraph", quietly = TRUE)) {
      stop(
        "The 'igraph' package is required for igraph input. ",
        "Install it with install.packages(\"igraph\").",
        call. = FALSE
      )
    }
    if (length(igraph_weight) != igraph::ecount(graph)) {
      stop("`e.weights` must have one value per edge.", call. = FALSE)
    }
    if (anyNA(igraph_weight)) {
      stop("`e.weights` cannot contain missing values.", call. = FALSE)
    }
    edge_weight_attr <- ".infomap_e_weights"
    while (edge_weight_attr %in% igraph::edge_attr_names(graph)) {
      edge_weight_attr <- paste0(".", edge_weight_attr)
    }
    graph <- igraph::set_edge_attr(graph, edge_weight_attr, value = igraph_weight)
    igraph_weight <- edge_weight_attr
  }
  im$add_igraph(graph, weight = igraph_weight)
}

.normalize_edge_list <- function(edges, weight) {
  if (!is.data.frame(edges) && !is.matrix(edges)) {
    stop("`edges` must be a data.frame, matrix, or igraph graph.", call. = FALSE)
  }
  if (ncol(edges) < 2L) {
    stop("`edges` must have at least two columns (source, target).", call. = FALSE)
  }

  source <- if (is.matrix(edges)) edges[, 1L] else edges[[1L]]
  target <- if (is.matrix(edges)) edges[, 2L] else edges[[2L]]
  if (!is.numeric(source) || !is.numeric(target)) {
    stop("`edges` source/target columns must be numeric/integer.", call. = FALSE)
  }
  if (anyNA(source) || anyNA(target)) {
    stop("`edges` source/target columns cannot contain missing values.", call. = FALSE)
  }

  weight_values <- .edge_weights(edges, weight)
  if (!is.numeric(weight_values)) {
    stop("`weight` column must be numeric.", call. = FALSE)
  }
  if (anyNA(weight_values)) {
    stop("`weight` column cannot contain missing values.", call. = FALSE)
  }

  data.frame(
    source = source,
    target = target,
    weight = as.numeric(weight_values),
    stringsAsFactors = FALSE
  )
}

.edge_weights <- function(edges, weight) {
  if (identical(weight, FALSE)) {
    return(rep(1.0, nrow(edges)))
  }
  if (is.null(weight)) {
    if (ncol(edges) >= 3L) {
      return(if (is.matrix(edges)) edges[, 3L] else edges[[3L]])
    }
    return(rep(1.0, nrow(edges)))
  }

  if (is.character(weight) && length(weight) == 1L && is.data.frame(edges)) {
    if (!weight %in% names(edges)) {
      stop("`weight` column not found in `edges`.", call. = FALSE)
    }
    return(edges[[weight]])
  }

  if (is.numeric(weight) && length(weight) == 1L) {
    index <- as.integer(weight)
    if (is.na(index) || index < 1L || index > ncol(edges)) {
      stop("`weight` column index is out of bounds.", call. = FALSE)
    }
    return(if (is.matrix(edges)) edges[, index] else edges[[index]])
  }

  stop("`weight` must be NULL, FALSE, a column name, or a column index.", call. = FALSE)
}

.MULTILAYER_FULL_COLS  <- c("layer_from", "node_from", "layer_to", "node_to")
.MULTILAYER_INTRA_COLS <- c("layer", "node_from", "node_to")

.add_multilayer_input <- function(im, edges, weight) {
  if (!is.data.frame(edges) && !is.matrix(edges)) {
    stop("`multilayer_edges` must be a data.frame, tibble, or matrix.",
         call. = FALSE)
  }
  if (nrow(edges) == 0L) {
    stop("`multilayer_edges` must have at least one row.", call. = FALSE)
  }

  format <- .detect_multilayer_format(edges)
  if (format == "full") {
    normalized <- .normalize_multilayer_full(edges, weight)
    im$add_multilayer_links(normalized)
  } else {
    normalized <- .normalize_multilayer_intra(edges, weight)
    im$add_multilayer_intra_links(normalized)
  }
  invisible(NULL)
}

.detect_multilayer_format <- function(edges) {
  if (is.matrix(edges)) {
    nc <- ncol(edges)
    if (nc %in% c(4L, 5L)) return("full")
    if (nc %in% c(3L, 4L)) return("intra")
    stop(
      "Multilayer matrix input must have 3-4 columns ",
      "(layer, node_from, node_to, [weight]) or 4-5 columns ",
      "(layer_from, node_from, layer_to, node_to, [weight]).",
      call. = FALSE
    )
  }

  cols <- names(edges)
  has_full  <- all(.MULTILAYER_FULL_COLS  %in% cols)
  has_intra <- all(.MULTILAYER_INTRA_COLS %in% cols) &&
    !any(c("layer_from", "layer_to") %in% cols)

  if (has_full)  return("full")
  if (has_intra) return("intra")

  stop(
    "`multilayer_edges` must have either columns ",
    "{layer_from, node_from, layer_to, node_to} for a full multilayer ",
    "edge list, or {layer, node_from, node_to} for an intra-layer-only ",
    "edge list. An optional `weight` column is supported in both cases.",
    call. = FALSE
  )
}

.normalize_multilayer_full <- function(edges, weight) {
  if (is.matrix(edges)) {
    layer_from <- edges[, 1L]
    node_from  <- edges[, 2L]
    layer_to   <- edges[, 3L]
    node_to    <- edges[, 4L]
    weights <- .multilayer_weights(edges, weight, default_index = 5L)
  } else {
    layer_from <- edges[["layer_from"]]
    node_from  <- edges[["node_from"]]
    layer_to   <- edges[["layer_to"]]
    node_to    <- edges[["node_to"]]
    weights <- .multilayer_weights(
      edges, weight,
      reserved = .MULTILAYER_FULL_COLS
    )
  }

  .check_multilayer_ids(
    list(
      layer_from = layer_from, node_from = node_from,
      layer_to = layer_to, node_to = node_to
    )
  )

  data.frame(
    layer_from = layer_from,
    node_from  = node_from,
    layer_to   = layer_to,
    node_to    = node_to,
    weight     = as.numeric(weights),
    stringsAsFactors = FALSE
  )
}

.normalize_multilayer_intra <- function(edges, weight) {
  if (is.matrix(edges)) {
    layer     <- edges[, 1L]
    node_from <- edges[, 2L]
    node_to   <- edges[, 3L]
    weights <- .multilayer_weights(edges, weight, default_index = 4L)
  } else {
    layer     <- edges[["layer"]]
    node_from <- edges[["node_from"]]
    node_to   <- edges[["node_to"]]
    weights <- .multilayer_weights(
      edges, weight,
      reserved = .MULTILAYER_INTRA_COLS
    )
  }

  .check_multilayer_ids(
    list(layer = layer, node_from = node_from, node_to = node_to)
  )

  data.frame(
    layer     = layer,
    node_from = node_from,
    node_to   = node_to,
    weight    = as.numeric(weights),
    stringsAsFactors = FALSE
  )
}

.check_multilayer_ids <- function(cols) {
  for (nm in names(cols)) {
    v <- cols[[nm]]
    if (!is.numeric(v)) {
      stop("`", nm, "` column must be numeric/integer.", call. = FALSE)
    }
    if (anyNA(v)) {
      stop("`", nm, "` column cannot contain missing values.", call. = FALSE)
    }
  }
}

.multilayer_weights <- function(edges, weight,
                                reserved = NULL,
                                default_index = NULL) {
  n <- nrow(edges)

  if (identical(weight, FALSE)) {
    return(rep(1.0, n))
  }

  if (is.null(weight)) {
    # Auto-detect: prefer a column literally named "weight" on data frames,
    # else fall back to a trailing positional column on matrices.
    if (is.data.frame(edges) && "weight" %in% names(edges)) {
      w <- edges[["weight"]]
    } else if (is.matrix(edges) && !is.null(default_index) &&
               ncol(edges) >= default_index) {
      w <- edges[, default_index]
    } else if (is.data.frame(edges) && !is.null(reserved)) {
      extras <- setdiff(names(edges), reserved)
      if (length(extras) == 1L) {
        w <- edges[[extras]]
      } else {
        return(rep(1.0, n))
      }
    } else {
      return(rep(1.0, n))
    }
  } else if (is.character(weight) && length(weight) == 1L &&
             is.data.frame(edges)) {
    if (!weight %in% names(edges)) {
      stop("`weight` column not found in `multilayer_edges`.", call. = FALSE)
    }
    w <- edges[[weight]]
  } else if (is.numeric(weight) && length(weight) == 1L) {
    index <- as.integer(weight)
    if (is.na(index) || index < 1L || index > ncol(edges)) {
      stop("`weight` column index is out of bounds.", call. = FALSE)
    }
    w <- if (is.matrix(edges)) edges[, index] else edges[[index]]
  } else {
    stop(
      "`weight` must be NULL, FALSE, a column name, or a column index.",
      call. = FALSE
    )
  }

  if (!is.numeric(w)) {
    stop("`weight` column must be numeric.", call. = FALSE)
  }
  if (anyNA(w)) {
    stop("`weight` column cannot contain missing values.", call. = FALSE)
  }
  w
}
