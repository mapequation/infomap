# igraph integration. Translated from interfaces/python/src/infomap/_networkx.py
# and adapted to the R igraph package.

InfomapClass$set(
  "public",
  "add_igraph",
  function(g,
           weight = "weight",
           phys_id = "phys_id",
           layer_id = "layer_id",
           multilayer_inter_intra_format = TRUE) {
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
    has_phys <- phys_id %in% igraph::vertex_attr_names(g)
    has_layer <- layer_id %in% igraph::vertex_attr_names(g)
    has_weight <- weight %in% igraph::edge_attr_names(g)

    vertex_names <- igraph::V(g)$name
    if (is.null(vertex_names)) {
      vertex_names <- as.character(seq_len(igraph::vcount(g)))
    }

    # Build a 0-indexed mapping from vertex index -> internal id and
    # propagate names if the graph has no phys_id attribute.
    internal_ids <- seq_len(igraph::vcount(g)) - 1L
    mapping <- stats::setNames(vertex_names, as.character(internal_ids))

    if (has_phys) {
      phys <- as.integer(igraph::vertex_attr(g, phys_id))
    } else {
      phys <- internal_ids
    }

    if (!has_phys && is.character(vertex_names)) {
      for (i in seq_len(igraph::vcount(g))) {
        self$add_node(internal_ids[i], name = vertex_names[i])
      }
    }

    # Vertex names are captured in `mapping` above; the edge list uses
    # 0-indexed integer ids (Infomap's convention) regardless.
    edges <- igraph::as_edgelist(g, names = FALSE) - 1L
    weights <- if (has_weight) {
      as.numeric(igraph::edge_attr(g, weight))
    } else {
      rep(1.0, nrow(edges))
    }

    if (has_phys && has_layer) {
      # Multilayer state network.
      layers <- as.integer(igraph::vertex_attr(g, layer_id))
      for (i in seq_len(igraph::vcount(g))) {
        self$add_state_node(internal_ids[i], phys[i])
      }
      for (e in seq_len(nrow(edges))) {
        s <- edges[e, 1L] + 1L
        t <- edges[e, 2L] + 1L
        if (isTRUE(multilayer_inter_intra_format)) {
          if (layers[s] == layers[t]) {
            self$add_multilayer_intra_link(layers[s], phys[s], phys[t], weights[e])
          } else {
            # add_multilayer_inter_link models the same physical node
            # across layers; diagonal edges (different layer AND
            # different physical node) cannot be represented.
            if (phys[s] != phys[t]) {
              stop(
                "Multilayer intra/inter format does not support diagonal links ",
                "(edge between different physical nodes in different layers). ",
                "Use `multilayer_inter_intra_format = FALSE`.",
                call. = FALSE
              )
            }
            self$add_multilayer_inter_link(layers[s], phys[s], layers[t], weights[e])
          }
        } else {
          self$add_multilayer_link(c(layers[s], phys[s]), c(layers[t], phys[t]), weights[e])
        }
      }
    } else if (has_phys) {
      # State network without explicit layer info.
      for (i in seq_len(igraph::vcount(g))) {
        self$add_state_node(internal_ids[i], phys[i])
      }
      for (e in seq_len(nrow(edges))) {
        self$add_link(edges[e, 1L], edges[e, 2L], weights[e])
      }
    } else {
      # Plain network.
      for (e in seq_len(nrow(edges))) {
        self$add_link(edges[e, 1L], edges[e, 2L], weights[e])
      }
    }

    if (is_directed) {
      # Mirror Python: remember that the source graph is directed. run()
      # injects --directed unless the user has set a flow model (via
      # opts at construction/run, or via raw args).
      private$.directed_from_igraph <- TRUE
    }

    invisible(mapping)
  }
)

# R6 method `as_communities(g)`: convert the Infomap partition to an
# igraph `communities` object (from `igraph::make_clusters()`),
# compatible with `modularity()`, `membership()`, and `plot()`. `g`
# must be the same igraph graph passed to `add_igraph()`.
InfomapClass$set(
  "public",
  "as_communities",
  function(g) {
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
    # im$modules returns a named integer vector: node_id -> 0-indexed module.
    # igraph's make_clusters expects a 1-indexed membership vector aligned
    # with V(g) order, so we add 1L and reorder by vertex sequence.
    mods <- self$modules
    n <- igraph::vcount(g)
    vertex_ids <- as.character(seq_len(n) - 1L)
    membership <- as.integer(mods[vertex_ids])
    igraph::make_clusters(
      g,
      membership  = membership,
      algorithm   = "Infomap",
      modularity  = FALSE
    )
  }
)
