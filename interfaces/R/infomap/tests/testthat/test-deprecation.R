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
