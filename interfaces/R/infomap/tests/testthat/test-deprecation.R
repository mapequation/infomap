test_that("include_self_links emits a deprecation warning", {
  expect_warning(
    construct_args(NULL, infomap_options(include_self_links = TRUE)),
    "deprecated",
    class = "deprecatedWarning"
  )
})

test_that("no_self_links renders --no-self-links", {
  rendered <- construct_args(NULL, infomap_options(no_self_links = TRUE))
  expect_match(rendered, "--no-self-links")
})

test_that("options the 3.0 policy removes from R warn when set away from their default", {
  expect_warning(
    construct_args(NULL, infomap_options(threads = 4)),
    "threads leaves the infomap R surface in 3.0",
    class = "deprecatedWarning"
  )
  expect_warning(
    construct_args(NULL, infomap_options(print_config_fingerprint = TRUE)),
    "print_config_fingerprint leaves the infomap R surface in 3.0",
    class = "deprecatedWarning"
  )
})

test_that("defaults and kept options stay quiet", {
  # silent and verbosity_level are `keep` for R. The R option-surface decision
  # (#757) settled that 3.0 changes the R option surface only and that these two
  # stay: the R default is silent = FALSE, and making the library quiet by
  # default is a behaviour change, not an API cleanup.
  expect_silent(construct_args(NULL, infomap_options(num_trials = 3)))
  expect_silent(construct_args(
    NULL,
    infomap_options(silent = TRUE, verbosity_level = 2L)
  ))
  # The rendered arguments are unaffected by the notice.
  expect_match(
    suppressWarnings(construct_args(NULL, infomap_options(threads = 4))),
    "--threads 4"
  )
})

# ----------------------------------------------------------------------------
# The tree-level selector (#757): `level` is canonical from 2.16, `depth_level`
# is a deprecated alias that leaves the R surface in 3.0.
# ----------------------------------------------------------------------------

fitted_infomap <- function() {
  im <- Infomap(silent = TRUE, seed = 123, num_trials = 1)
  im$add_links(list(
    c(1, 2),
    c(2, 3),
    c(3, 1),
    c(4, 5),
    c(5, 6),
    c(6, 4),
    c(3, 4)
  ))
  im$run()
  im
}

test_that("level is the selector and the package never warns about itself", {
  im <- fitted_infomap()
  expect_silent(im$get_modules(level = 1L))
  expect_silent(im$get_nodes(level = 1L))
  expect_silent(im$get_modules())
  # These route through get_nodes/get_modules internally; a caller must not
  # hear about the package's own plumbing.
  expect_silent(as.data.frame(im))
  expect_silent(im$modules)
})

test_that("depth_level still works and announces its removal", {
  im <- fitted_infomap()
  for (method in c("get_modules", "get_nodes")) {
    expect_warning(
      im[[method]](depth_level = 1L),
      "depth_level is deprecated as the tree-level selector",
      class = "deprecatedWarning"
    )
  }
  # Same answer through both spellings.
  expect_equal(
    suppressWarnings(im$get_modules(depth_level = -1L)),
    im$get_modules(level = -1L)
  )
})

test_that("as.data.frame and write_clu take level, with depth_level deprecated", {
  im <- fitted_infomap()
  expect_warning(
    as.data.frame(im, depth_level = 1L),
    "depth_level is deprecated",
    class = "deprecatedWarning"
  )
  expect_equal(
    suppressWarnings(as.data.frame(im, depth_level = -1L)),
    as.data.frame(im, level = -1L)
  )

  path <- tempfile(fileext = ".clu")
  on.exit(unlink(path), add = TRUE)
  expect_silent(im$write_clu(path, level = 1L))
  expect_warning(
    im$write_clu(path, depth_level = 1L),
    "depth_level is deprecated",
    class = "deprecatedWarning"
  )
})

test_that("passing both spellings is an error rather than a silent choice", {
  im <- fitted_infomap()
  expect_error(
    im$get_modules(level = -1L, depth_level = 1L),
    "Both `level` and `depth_level`"
  )
})

test_that("max_tree_depth is a deprecated alias of num_levels", {
  im <- fitted_infomap()
  expect_silent(im$num_levels)
  expect_warning(
    im$max_tree_depth,
    "max_tree_depth is a deprecated alias of num_levels",
    class = "deprecatedWarning"
  )
  expect_equal(suppressWarnings(im$max_tree_depth), im$num_levels)
})
