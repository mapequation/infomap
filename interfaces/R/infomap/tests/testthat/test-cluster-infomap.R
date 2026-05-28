test_that("cluster_infomap accepts data.frame edge lists", {
  edges <- data.frame(
    source = c(1, 1, 2, 3, 4, 4, 5),
    target = c(2, 3, 3, 4, 5, 6, 6)
  )

  result <- cluster_infomap(
    edges,
    silent = TRUE,
    num_trials = 5,
    two_level = TRUE
  )

  expect_s3_class(result, "infomap_result")
  expect_s3_class(result$model, "Infomap")
  expect_s3_class(result$nodes, "data.frame")
  expect_equal(result$num_nodes, 6L)
  expect_equal(result$num_links, 7L)
  expect_equal(result$num_top_modules, 2L)
  expect_named(result$modules, as.character(1:6))
  expect_true(is.numeric(result$codelength))
  expect_true(all(c("node_id", "module_id", "flow") %in% names(result$nodes)))
})

test_that("cluster_infomap accepts matrix edge lists", {
  edges <- matrix(
    c(1, 2, 1, 3, 2, 3, 4, 5, 4, 6, 5, 6),
    ncol = 2L,
    byrow = TRUE
  )

  result <- cluster_infomap(
    edges,
    silent = TRUE,
    num_trials = 5,
    two_level = TRUE
  )

  expect_s3_class(result, "infomap_result")
  expect_equal(result$num_nodes, 6L)
  expect_equal(result$num_links, 6L)
  expect_setequal(result$nodes$node_id, 1:6)
})

test_that("cluster_infomap accepts weighted edge lists", {
  edges <- data.frame(
    source = c(1, 1, 2, 3, 4, 4, 5),
    target = c(2, 3, 3, 4, 5, 6, 6),
    w = c(2, 2, 2, 1, 3, 3, 3)
  )

  default_weight <- cluster_infomap(edges, silent = TRUE, two_level = TRUE)
  named_weight <- cluster_infomap(
    edges,
    weight = "w",
    silent = TRUE,
    two_level = TRUE
  )
  unweighted <- cluster_infomap(
    edges,
    weight = FALSE,
    silent = TRUE,
    two_level = TRUE
  )

  expect_equal(default_weight$num_links, 7L)
  expect_equal(named_weight$num_links, 7L)
  expect_equal(unweighted$num_links, 7L)
  expect_true(is.numeric(named_weight$codelength))
})

test_that("infomap_result methods expose node table and summary", {
  edges <- data.frame(source = c(1, 2, 3), target = c(2, 3, 1))
  result <- cluster_infomap(edges, silent = TRUE)

  printed <- capture.output(print(result))
  expect_match(printed[[1L]], "<infomap_result>", fixed = TRUE)
  expect_true(any(grepl("codelength:", printed, fixed = TRUE)))
  expect_s3_class(summary(result), "summary.infomap_result")
  expect_s3_class(as.data.frame(result), "data.frame")
  expect_equal(as.data.frame(result), result$nodes)
})

test_that("cluster_infomap accepts igraph input", {
  skip_if_not_installed("igraph")

  graph <- igraph::make_graph(
    c(1, 2, 1, 3, 2, 3, 4, 5, 4, 6, 5, 6),
    directed = FALSE
  )
  result <- cluster_infomap(
    graph,
    silent = TRUE,
    nb.trials = 5,
    two_level = TRUE
  )

  expect_s3_class(result, "infomap_result")
  expect_equal(
    names(result$mapping),
    as.character(seq_len(igraph::vcount(graph)))
  )
  expect_setequal(result$nodes$node_id, seq_len(igraph::vcount(graph)))

  communities <- as_communities(result, graph)
  expect_s3_class(communities, "communities")
  expect_length(igraph::membership(communities), igraph::vcount(graph))
})

test_that("cluster_infomap supports igraph-style edge weight alias", {
  skip_if_not_installed("igraph")

  graph <- igraph::make_graph(c(1, 2, 2, 3, 3, 1), directed = FALSE)
  igraph::E(graph)$strength <- c(1, 2, 3)
  result <- cluster_infomap(graph, e.weights = "strength", silent = TRUE)
  vector_result <- cluster_infomap(graph, e.weights = c(1, 2, 3), silent = TRUE)

  expect_s3_class(result, "infomap_result")
  expect_s3_class(vector_result, "infomap_result")
  expect_equal(result$num_links, 3L)
  expect_equal(vector_result$num_links, 3L)
})

test_that("cluster_infomap rejects conflicting or unsupported igraph-style aliases", {
  edges <- data.frame(source = c(1, 2), target = c(2, 1))

  expect_error(
    cluster_infomap(edges, weight = 3L, e.weights = 3L, silent = TRUE),
    "only one of `weight` and `e.weights`"
  )
  expect_error(
    cluster_infomap(edges, nb.trials = 2L, num_trials = 2L, silent = TRUE),
    "only one of `num_trials` and `nb.trials`"
  )
  expect_error(
    cluster_infomap(edges, v.weights = c(1, 1), silent = TRUE),
    "`v.weights` is not supported"
  )

  skip_if_not_installed("igraph")
  graph <- igraph::make_graph(c(1, 2, 2, 1), directed = FALSE)
  expect_error(
    cluster_infomap(graph, e.weights = c(1, 2, 3), silent = TRUE),
    "one value per edge"
  )
})

test_that("cluster_infomap rejects invalid edge-list inputs", {
  expect_error(
    cluster_infomap(data.frame(source = 1), silent = TRUE),
    "at least two columns"
  )
  expect_error(
    cluster_infomap(data.frame(source = "a", target = "b"), silent = TRUE),
    "numeric/integer"
  )
  expect_error(
    cluster_infomap(
      data.frame(source = 1, target = 2, w = "bad"),
      silent = TRUE
    ),
    "weight.*numeric"
  )
  expect_error(
    cluster_infomap(
      data.frame(source = 1, target = 2),
      weight = "missing",
      silent = TRUE
    ),
    "not found"
  )
})
