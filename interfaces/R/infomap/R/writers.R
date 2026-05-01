# Output writers. Translated from interfaces/python/src/infomap/_writers.py.

InfomapClass$set("public", "write_clu", function(filename, states = FALSE, depth_level = 1L) {
  private$.swig$writeClu(as.character(filename), as.logical(states), as.integer(depth_level))
  invisible(filename)
})

InfomapClass$set("public", "write_tree", function(filename, states = FALSE) {
  private$.swig$writeTree(as.character(filename), as.logical(states))
  invisible(filename)
})

InfomapClass$set("public", "write_flow_tree", function(filename, states = FALSE) {
  private$.swig$writeFlowTree(as.character(filename), as.logical(states))
  invisible(filename)
})

InfomapClass$set("public", "write_newick", function(filename, states = FALSE) {
  private$.swig$writeNewickTree(as.character(filename), as.logical(states))
  invisible(filename)
})

InfomapClass$set("public", "write_json", function(filename, states = FALSE) {
  private$.swig$writeJsonTree(as.character(filename), as.logical(states))
  invisible(filename)
})

InfomapClass$set("public", "write_csv", function(filename, states = FALSE) {
  private$.swig$writeCsvTree(as.character(filename), as.logical(states))
  invisible(filename)
})

InfomapClass$set("public", "write_pajek", function(filename, flow = FALSE) {
  private$.swig$network()$writePajekNetwork(as.character(filename), as.logical(flow))
  invisible(filename)
})

InfomapClass$set("public", "write_state_network", function(filename) {
  private$.swig$network()$writeStateNetwork(as.character(filename))
  invisible(filename)
})

InfomapClass$set("public", "write", function(filename, ...) {
  ext <- tolower(tools::file_ext(filename))
  ext <- switch(ext,
    "ftree" = "flow_tree",
    "nwk"   = "newick",
    "net"   = "pajek",
    ext
  )
  method_name <- paste0("write_", ext)
  if (!is.function(self[[method_name]])) {
    stop("No method found for writing ", ext, " files", call. = FALSE)
  }
  self[[method_name]](filename, ...)
})
