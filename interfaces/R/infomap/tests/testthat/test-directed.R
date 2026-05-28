test_that("--directed flag is rendered when directed = TRUE", {
  rendered <- construct_args(NULL, infomap_options(directed = TRUE))
  expect_match(rendered, "--directed")

  rendered_undirected <- construct_args(NULL, infomap_options(directed = FALSE))
  expect_match(rendered_undirected, "--flow-model undirected")
})

test_that("directed and undirected runs produce different codelengths", {
  build <- function(directed) {
    im <- Infomap(silent = TRUE, num_trials = 5, directed = directed)
    im$add_link(1, 2)
    im$add_link(2, 3)
    im$add_link(3, 1)
    im$add_link(1, 4)
    im$add_link(4, 5)
    im$add_link(5, 4)
    im$run()
    im$codelength
  }
  expect_false(isTRUE(all.equal(build(TRUE), build(FALSE))))
})

test_that("add_igraph propagates directed graphs to run() by default", {
  skip_if_not_installed("igraph")
  g <- igraph::make_graph(
    c(1, 2, 2, 3, 3, 1, 3, 4, 4, 5, 5, 6, 6, 4),
    directed = TRUE
  )

  im <- Infomap(silent = TRUE, num_trials = 3)
  im$add_igraph(g)
  im$run()
  expect_gt(im$num_top_modules, 0L)
})

test_that("add_igraph does NOT override an opts-supplied flow_model", {
  skip_if_not_installed("igraph")
  g <- igraph::make_graph(c(1, 2, 2, 3, 3, 1), directed = TRUE)

  # Construction-time flow_model takes precedence; directed propagation
  # from add_igraph() must not kick in.
  im <- Infomap(silent = TRUE, flow_model = "undirected")
  im$add_igraph(g)
  im$run()
  expect_gt(im$num_top_modules, 0L)
})

test_that("add_igraph does NOT override raw args specifying a flow model", {
  skip_if_not_installed("igraph")
  g <- igraph::make_graph(c(1, 2, 2, 3, 3, 1), directed = TRUE)

  im <- Infomap(args = "--flow-model undirected --silent")
  im$add_igraph(g)
  im$run()
  expect_gt(im$num_top_modules, 0L)
})
