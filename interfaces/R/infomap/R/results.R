# Result accessors. Translated from interfaces/python/src/infomap/_results.py.
#
# SWIG R does not auto-convert std::map / std::vector to native R types,
# so all of these methods walk iterators (which DO work over the SWIG
# barrier) and accumulate values into R vectors.

# ----------------------------------------
# Methods
# ----------------------------------------

InfomapClass$set("public", "get_modules", function(depth_level = 1L, states = FALSE) {
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
      ids[i] <- as.integer(if (isTRUE(states)) it$stateId else it$physicalId)
      modules[i] <- as.integer(it$moduleId())
    }
    it$stepForward()
  }
  ids <- ids[seq_len(i)]
  modules <- modules[seq_len(i)]
  names(modules) <- as.character(ids)
  modules
})

InfomapClass$set("public", "get_multilevel_modules", function(states = FALSE) {
  depth <- self$num_levels
  if (depth < 1L) {
    return(structure(list(), names = character()))
  }
  per_level <- lapply(seq_len(depth), function(d) self$get_modules(d, states))
  ids <- names(per_level[[1L]])
  out <- lapply(ids, function(id) {
    vapply(per_level, function(m) m[[id]], integer(1L))
  })
  names(out) <- ids
  out
})

InfomapClass$set("public", "get_nodes", function(depth_level = 1L, states = FALSE) {
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
    state_id  = state_id[seq_len(i)],
    node_id   = node_id[seq_len(i)],
    module_id = module_id[seq_len(i)],
    flow      = flow[seq_len(i)]
  )
  if (has_layer) out$layer_id <- layer_id[seq_len(i)]
  out
})

InfomapClass$set("public", "get_name", function(node_id, default = NULL) {
  name <- private$.swig$getName(as.integer(node_id))
  if (identical(name, "")) return(default)
  name
})

InfomapClass$set("public", "get_names", function() {
  # SWIG R returns std::map as an opaque pointer, so walk leaf nodes and
  # ask for each name individually.
  ids <- self$get_nodes(depth_level = 1L, states = TRUE)$node_id
  ids <- unique(ids)
  out <- vapply(ids, function(id) {
    name <- private$.swig$getName(as.integer(id))
    if (is.null(name)) "" else name
  }, character(1L))
  names(out) <- as.character(ids)
  out[nzchar(out)]
})


# ----------------------------------------
# Active bindings
# ----------------------------------------

InfomapClass$set("active", "modules", function() {
  self$get_modules(depth_level = 1L, states = FALSE)
})

InfomapClass$set("active", "multilevel_modules", function() {
  self$get_multilevel_modules(states = FALSE)
})

InfomapClass$set("active", "nodes", function() {
  self$get_nodes(depth_level = 1L, states = TRUE)
})

InfomapClass$set("active", "physical_nodes", function() {
  self$get_nodes(depth_level = 1L, states = FALSE)
})

InfomapClass$set("active", "names", function() {
  self$get_names()
})

# Note: per-link weight/flow extraction (Python's `links`/`flow_links`)
# is not exposed yet because SWIG R does not provide an iterable
# wrapper over the underlying IterWrapper / std::map containers.
# Use the Python or JS bindings for per-link flow extraction.
