test_that("get_modules returns a named integer vector", {
  im <- Infomap(silent = TRUE, num_trials = 3)
  im$add_links(list(
    c(1, 2),
    c(1, 3),
    c(2, 3),
    c(3, 4),
    c(4, 5),
    c(4, 6),
    c(5, 6)
  ))
  im$run()

  m <- im$get_modules()
  expect_type(m, "integer")
  expect_named(m)
  expect_length(m, 6L)
  expect_true(all(m %in% c(1L, 2L)))
})

test_that("get_multilevel_modules returns a list of integer paths", {
  im <- Infomap(silent = TRUE, num_trials = 3)
  im$add_links(list(
    c(1, 2),
    c(1, 3),
    c(2, 3),
    c(3, 4),
    c(4, 5),
    c(4, 6),
    c(5, 6)
  ))
  im$run()

  ml <- im$get_multilevel_modules()
  expect_type(ml, "list")
  expect_named(ml)
  expect_length(ml, 6L)
  expect_true(all(vapply(ml, is.integer, logical(1L))))
})

test_that("active bindings return current values without parens", {
  im <- Infomap(silent = TRUE)
  im$add_link(1, 2)
  im$add_link(2, 3)
  im$run()

  expect_type(im$codelength, "double")
  expect_type(im$num_top_modules, "integer")
  expect_type(im$num_links, "integer")
  expect_type(im$num_nodes, "integer")
})

test_that("entropy / perplexity / plogp compute the expected values", {
  entropy <- getFromNamespace("entropy", "infomap")
  perplexity <- getFromNamespace("perplexity", "infomap")
  plogp <- getFromNamespace("plogp", "infomap")

  expect_equal(entropy(c(0.5, 0.5)), 1)
  expect_equal(perplexity(c(0.5, 0.5)), 2)
  expect_equal(plogp(0), 0)
  expect_equal(plogp(0.5), -0.5)
})

test_that("multilayer_node returns a tagged integer pair", {
  m <- multilayer_node(1L, 2L)
  expect_s3_class(m, "multilayer_node")
  expect_equal(unname(m), c(1L, 2L))
  expect_named(m, c("layer_id", "node_id"))
})

test_that("num_levels is the tree depth, not the first-child branch's (#1036)", {
  # numLevels() in the C++ core walks only the first-child chain, so on a ragged
  # tree it reports one arbitrary branch's depth. num_levels used it, which also
  # made get_multilevel_modules() truncate the deeper branches.
  net <- tempfile(fileext = ".net")
  writeLines(
    c(
      "*Vertices",
      "1 \"A\"",
      "2 \"B\"",
      "3 \"C\"",
      "4 \"D\"",
      "5 \"E\"",
      "6 \"F\"",
      "*Edges",
      "1 2",
      "1 3",
      "2 3",
      "3 4",
      "4 5",
      "4 6",
      "5 6"
    ),
    net
  )

  # Ragged, with the shallow branch first: module 1 ends at depth 2, module 2 at
  # depth 3. --no-infomap keeps that shape instead of re-partitioning it.
  tree <- tempfile(fileext = ".tree")
  writeLines(
    c(
      "# path flow name state_id",
      "1:1 0.1 \"A\" 1",
      "1:2 0.1 \"B\" 2",
      "1:3 0.1 \"C\" 3",
      "2:1:1 0.1 \"D\" 4",
      "2:1:2 0.1 \"E\" 5",
      "2:2:1 0.1 \"F\" 6"
    ),
    tree
  )

  im <- Infomap(
    args = paste("--no-infomap --cluster-data", tree),
    silent = TRUE
  )
  im$read_file(net)
  im$run()

  # Precondition: the tree really is three deep, so this is not vacuous.
  expect_equal(im$max_tree_depth, 3L)
  expect_equal(im$num_levels, im$max_tree_depth)

  ml <- im$get_multilevel_modules()
  expect_length(ml, 6L)
  expect_true(all(vapply(ml, length, integer(1L)) == 3L))
})
