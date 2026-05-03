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
