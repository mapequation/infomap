# Node, state and layer ids cross into C++ as R integers: the SWIG layer coerces
# to integer range, so a value above .Machine$integer.max becomes NA and every
# such id collapses onto the same node -- silently partitioning a different
# network than the caller described. These tests pin the boundary to an error.

test_that("add_links rejects node ids above the representable range", {
  im <- Infomap(silent = TRUE, num_trials = 1L)
  edges <- data.frame(source = c(1, 2) + 3e9, target = c(2, 3) + 3e9)

  expect_error(im$add_links(edges), "2147483647", fixed = TRUE)
})

test_that("add_link rejects a single out-of-range node id", {
  im <- Infomap(silent = TRUE, num_trials = 1L)

  expect_error(im$add_link(3e9, 1), "2147483647", fixed = TRUE)
  expect_error(im$add_link(1, 3e9), "2147483647", fixed = TRUE)
})

test_that("add_node rejects an out-of-range node id", {
  im <- Infomap(silent = TRUE, num_trials = 1L)

  expect_error(im$add_node(2^31), "2147483647", fixed = TRUE)
})

test_that("node ids at the representable limit are accepted", {
  im <- Infomap(silent = TRUE, num_trials = 1L)

  expect_silent(im$add_link(2147483647, 1))
  expect_equal(im$num_nodes, 2L)
})

test_that("node ids are rejected rather than silently truncated or wrapped", {
  im <- Infomap(silent = TRUE, num_trials = 1L)

  expect_error(im$add_link(-1, 2), "negative", fixed = TRUE)
  expect_error(im$add_link(1.5, 2), "whole number", fixed = TRUE)
  expect_error(im$add_link(NA, 2), "missing", fixed = TRUE)
  expect_error(im$add_link(NaN, 2), "missing", fixed = TRUE)
  expect_error(im$add_link(Inf, 2), "finite", fixed = TRUE)
})

test_that("an out-of-range id no longer collapses a network into one node", {
  # The regression: the same topology with large ids partitioned as a single
  # node with codelength 0 instead of two modules.
  edges <- data.frame(
    source = c(1, 2, 3, 4, 5, 6, 3),
    target = c(2, 3, 1, 5, 6, 4, 4)
  )

  small <- Infomap(silent = TRUE, num_trials = 1L, seed = 1L)
  small$add_links(edges)
  reference <- small$run()

  expect_equal(reference$num_nodes, 6L)
  expect_gt(reference$codelength, 0)

  large <- Infomap(silent = TRUE, num_trials = 1L, seed = 1L)
  expect_error(large$add_links(edges + 3e9), "2147483647", fixed = TRUE)
})

test_that("multilayer ids are validated too", {
  im <- Infomap(silent = TRUE, num_trials = 1L)

  expect_error(
    im$add_multilayer_intra_link(1, 3e9, 2),
    "2147483647",
    fixed = TRUE
  )
  expect_error(
    im$add_multilayer_intra_link(3e9, 1, 2),
    "2147483647",
    fixed = TRUE
  )
})

test_that("state ids are validated too", {
  im <- Infomap(silent = TRUE, num_trials = 1L)

  expect_error(im$add_state_node(3e9, 1), "2147483647", fixed = TRUE)
})
