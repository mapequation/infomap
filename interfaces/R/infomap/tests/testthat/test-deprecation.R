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
