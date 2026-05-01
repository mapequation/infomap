test_that("add_igraph imports edges from an igraph graph", {
  skip_if_not_installed("igraph")

  g <- igraph::make_graph(c(1, 2, 1, 3, 2, 3, 4, 5, 4, 6, 5, 6), directed = FALSE)

  im <- Infomap(silent = TRUE, num_trials = 5)
  mapping <- im$add_igraph(g)
  im$run()

  expect_equal(im$num_top_modules, 2L)
  expect_true(im$num_links >= 6L)
})
