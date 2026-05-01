test_that("get_modules returns a named integer vector", {
  im <- Infomap(silent = TRUE, num_trials = 3)
  im$add_links(list(c(1, 2), c(1, 3), c(2, 3), c(3, 4), c(4, 5), c(4, 6), c(5, 6)))
  im$run()

  m <- im$get_modules()
  expect_type(m, "integer")
  expect_named(m)
  expect_length(m, 6L)
  expect_true(all(m %in% c(1L, 2L)))
})

test_that("get_multilevel_modules returns a list of integer paths", {
  im <- Infomap(silent = TRUE, num_trials = 3)
  im$add_links(list(c(1, 2), c(1, 3), c(2, 3), c(3, 4), c(4, 5), c(4, 6), c(5, 6)))
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
