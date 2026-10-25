test_that("write_tree round-trips through read_file", {
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
  expected_modules <- im$num_top_modules

  tmp <- tempfile(fileext = ".tree")
  on.exit(unlink(tmp), add = TRUE)
  im$write_tree(tmp)
  expect_true(file.exists(tmp))
  expect_gt(file.size(tmp), 0L)

  im2 <- Infomap(silent = TRUE, no_infomap = TRUE, cluster_data = tmp)
  im2$add_links(list(
    c(1, 2),
    c(1, 3),
    c(2, 3),
    c(3, 4),
    c(4, 5),
    c(4, 6),
    c(5, 6)
  ))
  im2$run()
  expect_equal(im2$num_top_modules, expected_modules)
})

test_that("write_clu writes a non-empty file", {
  im <- Infomap(silent = TRUE)
  im$add_links(list(c(1, 2), c(2, 3), c(3, 1)))
  im$run()

  tmp <- tempfile(fileext = ".clu")
  on.exit(unlink(tmp), add = TRUE)
  im$write_clu(tmp)
  expect_true(file.exists(tmp))
  expect_gt(file.size(tmp), 0L)
})

test_that("write_flow_tree writes a non-empty file", {
  im <- Infomap(silent = TRUE)
  im$add_links(list(c(1, 2), c(2, 3), c(3, 1)))
  im$run()

  tmp <- tempfile(fileext = ".ftree")
  on.exit(unlink(tmp), add = TRUE)
  im$write_flow_tree(tmp)
  expect_true(file.exists(tmp))
  expect_gt(file.size(tmp), 0L)
})

# ----------------------------------------------------------------------------
# A published artifact must not name an input file that no longer describes the
# network (#1026). The R mutators used to reach through $network() straight to
# the C++ Network, past the wrapper that drops the identity.
# ----------------------------------------------------------------------------

network_fixture <- function() {
  net <- tempfile(fileext = ".net")
  writeLines(
    c(
      "*Vertices 4",
      "1 \"a\"",
      "2 \"b\"",
      "3 \"c\"",
      "4 \"d\"",
      "*Edges",
      "1 2",
      "2 3",
      "3 1",
      "3 4"
    ),
    net
  )
  net
}

tree_header <- function(im) {
  out <- tempfile(fileext = ".tree")
  on.exit(unlink(out), add = TRUE)
  im$write_tree(out)
  readLines(out, warn = FALSE)
}

test_that("a run read from a file stamps the input fingerprint", {
  net <- network_fixture()
  on.exit(unlink(net), add = TRUE)

  im <- Infomap(silent = TRUE, num_trials = 1, seed = 123)
  im$read_file(net)
  im$run()
  expect_true(any(grepl("^# input fingerprint", tree_header(im))))
})

test_that("mutating after a read drops the input fingerprint", {
  net <- network_fixture()
  on.exit(unlink(net), add = TRUE)

  # add_link already went through the wrapper; this is the control.
  built <- Infomap(silent = TRUE, num_trials = 1, seed = 123)
  built$read_file(net)
  built$add_link(90, 91)
  built$run()
  expect_false(any(grepl("^# input fingerprint", tree_header(built))))

  # These two reached past the wrapper before #1026's follow-up.
  removed <- Infomap(silent = TRUE, num_trials = 1, seed = 123)
  removed$read_file(net)
  removed$remove_link(1, 2)
  removed$run()
  expect_false(any(grepl("^# input fingerprint", tree_header(removed))))

  with_meta <- Infomap(silent = TRUE, num_trials = 1, seed = 123)
  with_meta$read_file(net)
  with_meta$set_meta_data(1, 7)
  with_meta$run()
  expect_false(any(grepl("^# input fingerprint", tree_header(with_meta))))
})
