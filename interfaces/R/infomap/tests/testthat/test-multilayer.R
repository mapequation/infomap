test_that("multilayer intra/inter format clusters across layers", {
  im <- Infomap(silent = TRUE, num_trials = 3, two_level = TRUE)

  # Two layers, three nodes per layer, intra-layer triangles, inter-layer
  # links connecting matching physical nodes.
  for (layer in 1:2) {
    for (e in list(c(1, 2), c(2, 3), c(3, 1))) {
      im$add_multilayer_intra_link(layer, e[1], e[2], 1.0)
    }
  }
  for (n in 1:3) im$add_multilayer_inter_link(1, n, 2, 1.0)

  im$run()
  expect_true(im$have_memory)
  expect_gt(im$num_levels, 0L)

  df <- as.data.frame(im)
  expect_true("layer_id" %in% names(df))
  expect_setequal(unique(df$layer_id), c(1L, 2L))
})

expect_same_multilayer_links <- function(actual, expected) {
  expect_equal(actual$num_links, expected$num_links)
  expect_equal(actual$num_nodes, expected$num_nodes)
  expect_equal(actual$num_physical_nodes, expected$num_physical_nodes)
}

intra_links <- list(
  c(1, 1, 2, 1.0), c(1, 2, 3, 1.0), c(1, 3, 1, 1.0),
  c(2, 1, 2, 2.0), c(2, 2, 3, 2.0), c(2, 3, 1, 2.0)
)

inter_links <- list(
  c(1, 1, 2, 0.5), c(1, 2, 2, 0.5), c(1, 3, 2, 0.5)
)

expect_same_multilayer_run <- function(actual, expected) {
  actual$run()
  expected$run()
  expect_same_multilayer_links(actual, expected)
  expect_equal(actual$codelength, expected$codelength, tolerance = 1e-10)
}

test_that("add_multilayer_intra_links list input matches repeated add_multilayer_intra_link", {
  baseline <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  for (link in intra_links) {
    baseline$add_multilayer_intra_link(link[[1L]], link[[2L]], link[[3L]], link[[4L]])
  }

  im <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  im$add_multilayer_intra_links(intra_links)

  expect_same_multilayer_run(im, baseline)
})

test_that("add_multilayer_intra_links accepts matrix input", {
  links <- do.call(rbind, intra_links)

  baseline <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  for (i in seq_len(nrow(links))) {
    baseline$add_multilayer_intra_link(links[i, 1], links[i, 2], links[i, 3], links[i, 4])
  }

  im <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  im$add_multilayer_intra_links(links)

  expect_same_multilayer_run(im, baseline)
})

test_that("add_multilayer_intra_links accepts unweighted matrix input", {
  links <- do.call(rbind, intra_links)[, 1:3]

  baseline <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  for (i in seq_len(nrow(links))) {
    baseline$add_multilayer_intra_link(links[i, 1], links[i, 2], links[i, 3])
  }

  im <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  im$add_multilayer_intra_links(links)

  expect_same_multilayer_run(im, baseline)
})

test_that("add_multilayer_inter_links list input matches repeated add_multilayer_inter_link", {
  baseline <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  im <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  for (link in intra_links) {
    baseline$add_multilayer_intra_link(link[[1L]], link[[2L]], link[[3L]], link[[4L]])
    im$add_multilayer_intra_link(link[[1L]], link[[2L]], link[[3L]], link[[4L]])
  }
  for (link in inter_links) {
    baseline$add_multilayer_inter_link(link[[1L]], link[[2L]], link[[3L]], link[[4L]])
  }
  im$add_multilayer_inter_links(inter_links)

  expect_same_multilayer_run(im, baseline)
})

test_that("add_multilayer_inter_links accepts matrix input", {
  links <- do.call(rbind, inter_links)

  baseline <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  im <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  baseline$add_multilayer_intra_links(intra_links)
  im$add_multilayer_intra_links(intra_links)
  for (i in seq_len(nrow(links))) {
    baseline$add_multilayer_inter_link(links[i, 1], links[i, 2], links[i, 3], links[i, 4])
  }
  im$add_multilayer_inter_links(links)

  expect_same_multilayer_run(im, baseline)
})

test_that("add_multilayer_inter_links accepts unweighted matrix input", {
  links <- do.call(rbind, inter_links)[, 1:3]

  baseline <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  im <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  baseline$add_multilayer_intra_links(intra_links)
  im$add_multilayer_intra_links(intra_links)
  for (i in seq_len(nrow(links))) {
    baseline$add_multilayer_inter_link(links[i, 1], links[i, 2], links[i, 3])
  }
  im$add_multilayer_inter_links(links)

  expect_same_multilayer_run(im, baseline)
})

test_that("add_multilayer_links list input matches repeated add_multilayer_link", {
  links <- list(
    list(c(1, 1), c(1, 2), 2.0),
    list(c(1, 2), c(1, 3), 2.0),
    list(c(1, 3), c(2, 1), 1.0),
    list(c(2, 1), c(2, 2), 3.0),
    list(c(2, 2), c(2, 3), 3.0),
    list(c(2, 3), c(1, 1), 1.0)
  )

  baseline <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  for (link in links) {
    baseline$add_multilayer_link(link[[1L]], link[[2L]], link[[3L]])
  }

  im <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  im$add_multilayer_links(links)

  expect_same_multilayer_links(im, baseline)

  baseline$run()
  im$run()
  expect_equal(im$codelength, baseline$codelength, tolerance = 1e-10)
})

test_that("add_multilayer_links accepts matrix input through bulk path", {
  links <- matrix(
    c(1, 1, 1, 2, 2,
      1, 2, 2, 1, 1,
      2, 1, 2, 2, 3),
    ncol = 5L,
    byrow = TRUE
  )

  baseline <- Infomap(silent = TRUE)
  for (i in seq_len(nrow(links))) {
    baseline$add_multilayer_link(links[i, 1:2], links[i, 3:4], links[i, 5])
  }

  im <- Infomap(silent = TRUE)
  im$add_multilayer_links(links)

  expect_same_multilayer_links(im, baseline)
})

test_that("add_multilayer_links accepts unweighted matrix input", {
  links <- matrix(
    c(1, 1, 1, 2,
      1, 2, 2, 1,
      2, 1, 2, 2),
    ncol = 4L,
    byrow = TRUE
  )

  baseline <- Infomap(silent = TRUE)
  for (i in seq_len(nrow(links))) {
    baseline$add_multilayer_link(links[i, 1:2], links[i, 3:4])
  }

  im <- Infomap(silent = TRUE)
  im$add_multilayer_links(links)

  expect_same_multilayer_links(im, baseline)
})

test_that("add_igraph rejects diagonal multilayer links by default", {
  skip_if_not_installed("igraph")

  # Build an igraph with vertex attrs phys_id and layer_id, with a single
  # diagonal edge between (layer=1, phys=1) and (layer=2, phys=2).
  g <- igraph::make_empty_graph(4, directed = FALSE)
  g <- igraph::set_vertex_attr(g, "phys_id",  value = c(1, 2, 1, 2))
  g <- igraph::set_vertex_attr(g, "layer_id", value = c(1, 1, 2, 2))
  # Edge between vertex 1 (layer=1, phys=1) and vertex 4 (layer=2, phys=2)
  g <- igraph::add_edges(g, c(1, 4))

  im <- Infomap(silent = TRUE)
  expect_error(
    im$add_igraph(g),
    "diagonal links",
    fixed = TRUE
  )
})

test_that("add_igraph accepts intra/inter-compatible multilayer links", {
  skip_if_not_installed("igraph")

  g <- igraph::make_empty_graph(6, directed = TRUE)
  g <- igraph::set_vertex_attr(g, "phys_id",  value = c(1, 2, 3, 1, 2, 3))
  g <- igraph::set_vertex_attr(g, "layer_id", value = c(1, 1, 1, 2, 2, 2))
  g <- igraph::add_edges(g, c(
    1, 2, 2, 3, 3, 1,
    4, 5, 5, 6, 6, 4,
    1, 4, 2, 5, 3, 6
  ))

  im <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  expect_silent(im$add_igraph(g, multilayer_inter_intra_format = TRUE))
  im$run()

  expect_true(im$have_memory)
  expect_gt(im$num_links, 0L)
})

test_that("add_igraph supports non-numeric physical ids", {
  skip_if_not_installed("igraph")

  g <- igraph::make_empty_graph(4, directed = TRUE)
  g <- igraph::set_vertex_attr(
    g, "phys_id",
    value = factor(c("alpha", "beta", "alpha", "beta"))
  )
  g <- igraph::set_vertex_attr(g, "layer_id", value = c(1, 1, 2, 2))
  g <- igraph::add_edges(g, c(1, 2, 3, 4, 1, 3, 2, 4))

  im <- Infomap(silent = TRUE, num_trials = 1L, two_level = TRUE)
  mapping <- im$add_igraph(g)
  im$run()

  phys_mapping <- attr(mapping, "phys_id")
  expect_named(phys_mapping)
  expect_setequal(unname(phys_mapping), c("alpha", "beta"))
  expect_false(anyNA(im$nodes$node_id))

  communities <- im$as_communities(g)
  expect_s3_class(communities, "communities")
  expect_length(igraph::membership(communities), igraph::vcount(g))
  expect_false(anyNA(igraph::membership(communities)))
})

test_that("add_igraph accepts diagonal links with multilayer_inter_intra_format = FALSE", {
  skip_if_not_installed("igraph")
  g <- igraph::make_empty_graph(4, directed = FALSE)
  g <- igraph::set_vertex_attr(g, "phys_id",  value = c(1, 2, 1, 2))
  g <- igraph::set_vertex_attr(g, "layer_id", value = c(1, 1, 2, 2))
  g <- igraph::add_edges(g, c(1, 4))

  im <- Infomap(silent = TRUE)
  expect_silent(
    im$add_igraph(g, multilayer_inter_intra_format = FALSE)
  )
})
