test_that("add_igraph imports edges from an igraph graph", {
  skip_if_not_installed("igraph")

  g <- igraph::make_graph(
    c(1, 2, 1, 3, 2, 3, 4, 5, 4, 6, 5, 6),
    directed = FALSE
  )

  im <- Infomap(silent = TRUE, num_trials = 5)
  mapping <- im$add_igraph(g)
  im$run()

  expect_equal(names(mapping), as.character(seq_len(igraph::vcount(g))))
  expect_equal(unname(mapping), as.character(seq_len(igraph::vcount(g))))
  expect_equal(im$num_top_modules, 2L)
  expect_true(im$num_links >= 6L)
  expect_setequal(names(im$modules), as.character(seq_len(igraph::vcount(g))))
  expect_setequal(as.data.frame(im)$node_id, seq_len(igraph::vcount(g)))
})

test_that("add_igraph normalizes igraph weight inputs", {
  skip_if_not_installed("igraph")

  g <- igraph::make_graph(c(1, 2, 2, 3, 3, 1), directed = FALSE)
  igraph::E(g)$weight <- c(2, 3, 4)

  default_weight <- Infomap(silent = TRUE)
  expect_silent(default_weight$add_igraph(g, weight = NULL))

  ignored_weight <- Infomap(silent = TRUE)
  expect_silent(ignored_weight$add_igraph(g, weight = FALSE))

  no_weight <- igraph::delete_edge_attr(g, "weight")
  missing_weight <- Infomap(silent = TRUE)
  expect_silent(missing_weight$add_igraph(no_weight, weight = NULL))
  expect_silent(missing_weight$run())
})

test_that("add_igraph rejects invalid igraph weights", {
  skip_if_not_installed("igraph")

  non_numeric <- igraph::make_graph(c(1, 2, 2, 3), directed = FALSE)
  igraph::E(non_numeric)$weight <- c("a", "b")
  expect_error(
    Infomap(silent = TRUE)$add_igraph(non_numeric),
    "weight.*numeric"
  )

  missing <- igraph::make_graph(c(1, 2, 2, 3), directed = FALSE)
  igraph::E(missing)$weight <- c(1, NA)
  expect_error(
    Infomap(silent = TRUE)$add_igraph(missing),
    "weight.*missing"
  )
})

test_that("as_communities aligns membership with igraph vertex ids", {
  skip_if_not_installed("igraph")

  g <- igraph::make_graph(
    c(1, 2, 1, 3, 2, 3, 4, 5, 4, 6, 5, 6),
    directed = FALSE
  )

  im <- Infomap(silent = TRUE, num_trials = 5)
  im$add_igraph(g)
  im$run()

  communities <- im$as_communities(g)
  expect_s3_class(communities, "communities")
  expect_length(igraph::membership(communities), igraph::vcount(g))
  expect_false(anyNA(igraph::membership(communities)))
})

test_that("as_communities rejects graphs that do not match imported igraph vertices", {
  skip_if_not_installed("igraph")

  g <- igraph::make_graph(c(1, 2, 2, 3, 3, 1), directed = FALSE)
  other <- igraph::make_graph(c(1, 2, 2, 3, 3, 4), directed = FALSE)

  im <- Infomap(silent = TRUE)
  im$add_igraph(g)
  im$run()

  expect_error(
    im$as_communities(other),
    "same igraph graph"
  )
})

test_that("add_igraph preserves per-state names for state networks", {
  skip_if_not_installed("igraph")

  g <- igraph::make_graph(c(1, 2, 2, 1), directed = TRUE)
  igraph::V(g)$name <- c("state-a", "state-b")
  igraph::V(g)$phys_id <- c("alpha", "beta")

  im <- Infomap(silent = TRUE)
  im$add_igraph(g)

  expect_equal(im$get_state_name(1L), "state-a")
  expect_equal(im$get_state_name(2L), "state-b")
})

test_that("add_igraph does not synthesize state names when vertices are unnamed", {
  skip_if_not_installed("igraph")

  g <- igraph::make_graph(c(1, 2, 2, 1), directed = TRUE)
  igraph::V(g)$phys_id <- c("alpha", "beta")

  im <- Infomap(silent = TRUE)
  im$add_igraph(g)

  expect_null(im$get_state_name(1L))
  expect_null(im$get_state_name(2L))
})
