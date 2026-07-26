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
  rendered <- construct_args(
    NULL,
    infomap_options(teleportation_probability = 0.15)
  )
  expect_false(grepl("--teleportation-probability", rendered))
})

test_that("directed flag renders correctly", {
  expect_match(
    construct_args(NULL, infomap_options(directed = TRUE)),
    "--directed"
  )
  expect_match(
    construct_args(NULL, infomap_options(directed = FALSE)),
    "--flow-model undirected"
  )
  expect_false(grepl(
    "--directed|--flow-model",
    construct_args(NULL, infomap_options(directed = NULL))
  ))
})

test_that("verbosity_level renders -vv style flag", {
  expect_match(
    construct_args(NULL, infomap_options(verbosity_level = 2L)),
    "-vv"
  )
  expect_match(
    construct_args(NULL, infomap_options(verbosity_level = 3L)),
    "-vvv"
  )
  expect_false(grepl(
    "-vv",
    construct_args(NULL, infomap_options(verbosity_level = 1L))
  ))
})

test_that("output sequence renders comma-separated", {
  rendered <- construct_args(NULL, infomap_options(output = c("clu", "tree")))
  expect_match(rendered, "--output clu,tree")
})

test_that("fast_hierarchical_solution renders -F repetition", {
  expect_match(
    construct_args(NULL, infomap_options(fast_hierarchical_solution = 2L)),
    "-FF"
  )
  expect_match(
    construct_args(NULL, infomap_options(fast_hierarchical_solution = 3L)),
    "-FFF"
  )
})

test_that("multilayer_relax_limit defaults to NULL and is not rendered", {
  rendered <- construct_args(NULL, infomap_options())
  expect_false(grepl("--multilayer-relax-limit", rendered))
})

test_that("explicit multilayer_relax_limit = -1L is rendered", {
  rendered <- construct_args(
    NULL,
    infomap_options(multilayer_relax_limit = -1L)
  )
  expect_match(rendered, "--multilayer-relax-limit -1")
})

test_that("variable_markov_min_scale renders when non-default", {
  rendered <- construct_args(
    NULL,
    infomap_options(variable_markov_min_scale = 0.5)
  )
  expect_match(rendered, "--variable-markov-min-scale 0.5")
})

test_that("construct_args returns no leading whitespace", {
  rendered <- construct_args(NULL, infomap_options(silent = TRUE))
  expect_false(grepl("^\\s", rendered))
})

# The rendered string is split on whitespace by the engine, with no quoting, so a
# whitespace-bearing value does not stay one token: an out_name of "my run" used
# to render "--out-name my run", truncating the name to "my" and letting "run"
# become the output directory, at exit 0. Quoting cannot fix it -- the C++ side
# does not strip quotes -- so the value is refused instead.
test_that("whitespace in a string value is refused, naming the option", {
  expect_error(
    construct_args(NULL, infomap_options(out_name = "my run")),
    "out_name.*contains whitespace"
  )
  expect_error(
    construct_args(NULL, infomap_options(cluster_data = "a b.clu")),
    "cluster_data.*contains whitespace"
  )
  expect_error(
    construct_args(NULL, infomap_options(meta_data = "meta data.txt")),
    "meta_data.*contains whitespace"
  )
})

# The check runs on the rendered text, not on is.character(value), so a value that
# is not a character vector but renders with whitespace is caught too. A factor is
# the likely one (values arriving from a data.frame column); a POSIXct is the one
# nobody would think to enumerate -- as.character() gives it a space of its own.
test_that("whitespace is caught whatever type produced it", {
  expect_error(
    construct_args(NULL, infomap_options(out_name = factor("my run"))),
    "out_name = \"my run\" contains whitespace"
  )
  expect_error(
    construct_args(
      NULL,
      infomap_options(
        out_name = as.POSIXct("2026-07-26 08:30:00", tz = "UTC")
      )
    ),
    "contains whitespace"
  )
  # A type whose rendering has no whitespace is still rendered, as before.
  expect_match(
    construct_args(NULL, infomap_options(out_name = as.Date("2026-07-26"))),
    "--out-name 2026-07-26",
    fixed = TRUE
  )
})

test_that("whitespace-free values of the same options still render", {
  rendered <- construct_args(
    NULL,
    infomap_options(out_name = "my-run", cluster_data = "seed.clu")
  )
  expect_match(rendered, "--out-name my-run")
  expect_match(rendered, "--cluster-data seed.clu")
})

# format() honours getOption("digits"), which is 7 by default, so a fractional
# option reached the engine rounded: markov_time = 1/7 rendered as 0.1428571, a
# different parameter than the one requested and a different codelength than
# Python reports for the same input.
test_that("fractional values render with full round-trip precision", {
  rendered <- construct_args(NULL, infomap_options(markov_time = 1 / 7))
  expect_match(rendered, "--markov-time 0.14285714285714285", fixed = TRUE)

  rendered <- construct_args(
    NULL,
    infomap_options(markov_time = 1.2345678901234)
  )
  expect_match(rendered, "--markov-time 1.2345678901234", fixed = TRUE)

  # Round-tripping is the contract, not maximal digits: a value that is exactly
  # representable must not grow a tail of noise.
  rendered <- construct_args(
    NULL,
    infomap_options(teleportation_probability = 0.2)
  )
  expect_match(rendered, "--teleportation-probability 0.2", fixed = TRUE)
})

test_that("rendered numerics read back as the value that was requested", {
  for (value in c(1 / 7, 1 / 3, 0.15, 1e-9, 1.2345678901234)) {
    rendered <- construct_args(NULL, infomap_options(markov_time = value))
    text <- sub("^.*--markov-time ", "", rendered)
    expect_identical(as.numeric(text), value)
  }
})

# as.integer() returns NA above INT_MAX, so `value == as.integer(value)` was NA
# and `if (NA)` raised "missing value where TRUE/FALSE needed" for values the CLI
# accepts: the affected options are unsigned int in C++.
test_that("integer values above INT_MAX render instead of erroring", {
  expect_match(
    construct_args(NULL, infomap_options(seed = 2^31)),
    "--seed 2147483648",
    fixed = TRUE
  )
  expect_match(
    construct_args(NULL, infomap_options(seed = 2^32 - 1)),
    "--seed 4294967295",
    fixed = TRUE
  )
  # No scientific notation and no decimal point for whole values.
  expect_match(
    construct_args(NULL, infomap_options(seed = 3e9)),
    "--seed 3000000000",
    fixed = TRUE
  )
})

# Shape mistakes used to fail in two unhelpful ways: a length > 1 value hit R's
# "the condition has length > 1" from inside the renderer, and an NA passed every
# check silently -- grepl() returns FALSE for NA_character_, not NA -- so the
# literal string "NA" was rendered into the argument string for the engine to
# parse as a name or a number.
test_that("a non-scalar option value is refused, naming the option", {
  expect_error(
    construct_args(NULL, infomap_options(out_name = c("a", "b"))),
    "out_name must be a single value, got length 2"
  )
  expect_error(
    construct_args(NULL, infomap_options(markov_time = c(1.5, 2.5))),
    "markov_time must be a single value, got length 2"
  )
})

# is.na() is TRUE for NaN as well, but they are not the same mistake: NA is R's
# missing marker with no counterpart in the Python or JS bindings, while NaN is a
# number those bindings also hand to the engine. Rejecting NaN here would make R
# stricter than its siblings for no reason a caller could predict.
test_that("NaN is left to the engine, which rejects it by name", {
  rendered <- construct_args(NULL, infomap_options(markov_time = NaN))
  expect_match(rendered, "--markov-time NaN", fixed = TRUE)
})

test_that("an NA option value is refused rather than rendered as \"NA\"", {
  expect_error(
    construct_args(NULL, infomap_options(out_name = NA_character_)),
    "out_name must not be NA"
  )
  expect_error(
    construct_args(NULL, infomap_options(markov_time = NA_real_)),
    "markov_time must not be NA"
  )
  expect_error(
    construct_args(NULL, infomap_options(seed = NA_integer_)),
    "seed must not be NA"
  )
})

test_that("the comma-list output option still accepts a vector", {
  # output is joined before it reaches format_value, so the scalar requirement
  # must not reach it.
  expect_match(
    construct_args(NULL, infomap_options(output = c("clu", "tree"))),
    "--output clu,tree",
    fixed = TRUE
  )
})
