test_that("Infomap clusters two triangles into two modules", {
  im <- Infomap(silent = TRUE, num_trials = 5, two_level = TRUE)
  im$add_links(list(
    c(1, 2), c(1, 3), c(2, 3),
    c(3, 4),
    c(4, 5), c(4, 6), c(5, 6)
  ))
  im$run()

  expect_equal(im$num_top_modules, 2L)
  expect_gt(im$codelength, 1.5)
  expect_lt(im$codelength, 3.0)
  expect_equal(im$num_links, 7L)
  expect_equal(im$num_nodes, 6L)

  modules <- im$modules
  expect_equal(length(modules), 6L)
  expect_setequal(unique(modules), c(1L, 2L))
  expect_named(modules, as.character(1:6))
})

test_that("add_links accepts matrix input through bulk path", {
  im <- Infomap(silent = TRUE)
  links <- matrix(
    c(1, 2, 2,
      2, 3, 1,
      3, 1, 1),
    ncol = 3L,
    byrow = TRUE
  )

  im$add_links(links)
  im$run()

  expect_equal(im$num_links, 3L)
  expect_equal(im$num_nodes, 3L)
})

test_that("add_links list input matches repeated add_link", {
  links <- list(c(1, 2, 2), c(1, 3, 2), c(2, 3, 2), c(3, 4, 1),
                c(4, 5, 3), c(4, 6, 3), c(5, 6, 3))

  baseline <- Infomap(silent = TRUE, num_trials = 3, two_level = TRUE)
  for (link in links) {
    baseline$add_link(link[[1L]], link[[2L]], link[[3L]])
  }
  baseline$run()

  im <- Infomap(silent = TRUE, num_trials = 3, two_level = TRUE)
  im$add_links(links)
  im$run()

  expect_equal(im$num_links, baseline$num_links)
  expect_equal(im$num_nodes, baseline$num_nodes)
  expect_equal(im$codelength, baseline$codelength, tolerance = 1e-10)
  expect_equal(im$modules, baseline$modules)
})

test_that("Infomap accepts named-argument keyword overrides", {
  opts <- infomap_options(num_trials = 7L, silent = TRUE, two_level = TRUE)
  expect_true(is.list(opts))
  expect_equal(opts$num_trials, 7L)
  expect_true(opts$silent)
  expect_true(opts$two_level)

  im <- Infomap(opts = opts)
  expect_s3_class(im, "Infomap")
  expect_s3_class(im, "R6")

  # Override at construction time
  im2 <- Infomap(opts = opts, num_trials = 3L)
  expect_s3_class(im2, "Infomap")
})

test_that("as.data.frame returns one row per leaf node", {
  im <- Infomap(silent = TRUE)
  im$add_links(list(c(1, 2), c(2, 3), c(3, 1), c(3, 4), c(4, 5), c(5, 6), c(6, 4)))
  im$run()
  df <- as.data.frame(im)
  expect_s3_class(df, "data.frame")
  expect_false(inherits(df, "tbl_df"))
  expect_equal(nrow(df), 6L)
  expect_true(all(c("node_id", "module_id", "flow") %in% names(df)))
  expect_equal(sum(df$flow), 1, tolerance = 1e-6)
})

test_that("named arguments mirror Infomap CLI flags", {
  args <- construct_args(NULL, infomap_options(silent = TRUE, two_level = TRUE, num_trials = 4L))
  expect_match(args, "--silent")
  expect_match(args, "--two-level")
  expect_match(args, "--num-trials 4")
})

test_that("unknown options raise a helpful error", {
  expect_error(infomap_options(not_a_flag = TRUE), "Unknown infomap options")
})
