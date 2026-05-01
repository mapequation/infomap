test_that("default options render to an empty CLI string", {
  expect_equal(construct_args(NULL, infomap_options()), "")
})

test_that("explicit args are preserved when no options are set", {
  expect_equal(construct_args("--two-level"), "--two-level")
})

test_that("non-default values are rendered", {
  opts <- infomap_options(seed = 42L, num_trials = 5L, silent = TRUE)
  rendered <- construct_args(NULL, opts)
  expect_match(rendered, "--seed 42")
  expect_match(rendered, "--num-trials 5")
  expect_match(rendered, "--silent")
})

test_that("default values are skipped (not rendered)", {
  rendered <- construct_args(NULL, infomap_options(seed = 123L))
  expect_false(grepl("--seed", rendered))
  rendered <- construct_args(NULL, infomap_options(teleportation_probability = 0.15))
  expect_false(grepl("--teleportation-probability", rendered))
})

test_that("directed flag renders correctly", {
  expect_match(construct_args(NULL, infomap_options(directed = TRUE)), "--directed")
  expect_match(construct_args(NULL, infomap_options(directed = FALSE)), "--flow-model undirected")
  expect_false(grepl("--directed|--flow-model", construct_args(NULL, infomap_options(directed = NULL))))
})

test_that("verbosity_level renders -vv style flag", {
  expect_match(construct_args(NULL, infomap_options(verbosity_level = 2L)), "-vv")
  expect_match(construct_args(NULL, infomap_options(verbosity_level = 3L)), "-vvv")
  expect_false(grepl("-vv", construct_args(NULL, infomap_options(verbosity_level = 1L))))
})

test_that("output sequence renders comma-separated", {
  rendered <- construct_args(NULL, infomap_options(output = c("clu", "tree")))
  expect_match(rendered, "--output clu,tree")
})

test_that("fast_hierarchical_solution renders -F repetition", {
  expect_match(construct_args(NULL, infomap_options(fast_hierarchical_solution = 2L)), "-FF")
  expect_match(construct_args(NULL, infomap_options(fast_hierarchical_solution = 3L)), "-FFF")
})
