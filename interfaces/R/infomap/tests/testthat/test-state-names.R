make_state_network_file <- function() {
  net <- tempfile(fileext = ".net")
  writeLines(
    c(
      "*Vertices 5",
      "1 \"i\"",
      "2 \"j\"",
      "3 \"k\"",
      "4 \"l\"",
      "5 \"m\"",
      "*States",
      "1 1 \"a_i\"",
      "2 2 \"b_j\"",
      "3 3 \"c_k\"",
      "4 1 \"d_i\"",
      "5 4 \"e_l\"",
      "6 5 \"f_m\"",
      "*Links",
      "1 2 0.8",
      "1 3 0.8",
      "1 5 0.2",
      "1 6 0.2",
      "2 1 1",
      "2 3 1",
      "3 1 1",
      "3 2 1",
      "4 5 0.8",
      "4 6 0.8",
      "4 2 0.2",
      "4 3 0.2",
      "5 4 1",
      "5 6 1",
      "6 4 1",
      "6 5 1"
    ),
    net
  )
  net
}

test_that("state node names are exposed via get_state_names() and the data.frame", {
  net <- make_state_network_file()
  on.exit(unlink(net))

  im <- Infomap(silent = TRUE, num_trials = 1L)
  im$read_file(net)
  im$run()
  expect_true(im$have_memory)

  # New accessor: state id -> state-node name.
  state_names <- im$get_state_names()
  expect_setequal(
    unname(state_names),
    c("a_i", "b_j", "c_k", "d_i", "e_l", "f_m")
  )
  # Physical names stay separate, keyed by node id.
  expect_setequal(unname(im$get_names()), c("i", "j", "k", "l", "m"))

  df <- as.data.frame(im, states = TRUE)
  expect_true("state_name" %in% names(df))
  # "name" is the physical name; "state_name" the per-state-node name.
  expect_setequal(unique(df$name), c("i", "j", "k", "l", "m"))
  expect_setequal(df$state_name, c("a_i", "b_j", "c_k", "d_i", "e_l", "f_m"))

  # State nodes 1 and 4 are both physical node 1: the physical name collapses
  # them to "i", while state_name keeps them distinct.
  node1 <- df[df$node_id == 1L, ]
  expect_setequal(node1$name, "i")
  expect_setequal(node1$state_name, c("a_i", "d_i"))
})

test_that("as.data.frame omits state_name for first-order networks", {
  im <- Infomap(silent = TRUE, num_trials = 1L)
  im$add_links(list(
    c(1, 2),
    c(2, 3),
    c(3, 1),
    c(3, 4),
    c(4, 5),
    c(5, 6),
    c(6, 4)
  ))
  im$run()

  expect_false(im$have_memory)
  expect_length(im$get_state_names(), 0L)

  df <- as.data.frame(im)
  expect_false("state_name" %in% names(df))
})

test_that("add_state_node accepts an optional per-state name", {
  im <- Infomap(silent = TRUE)
  im$add_state_node(1L, 1L, name = "a")
  im$add_state_node(2L, 1L)

  expect_equal(im$get_state_name(1L), "a")
  expect_null(im$get_state_name(2L))
})
