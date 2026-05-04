test_that("add_links rejects character columns in a data.frame", {
  im <- Infomap(silent = TRUE)
  bad <- data.frame(s = c("a", "b"), t = c("c", "d"), stringsAsFactors = FALSE)
  expect_error(
    im$add_links(bad),
    "numeric/integer",
    fixed = TRUE
  )
})

test_that("add_links rejects a non-numeric weight column", {
  im <- Infomap(silent = TRUE)
  bad <- data.frame(s = c(1, 2), t = c(2, 3), w = c("a", "b"),
                    stringsAsFactors = FALSE)
  expect_error(im$add_links(bad), "weight column must be numeric")
})

test_that("add_links rejects a 4-column data.frame", {
  im <- Infomap(silent = TRUE)
  bad <- data.frame(a = 1, b = 2, c = 3, d = 4)
  expect_error(im$add_links(bad), "2 or 3 columns")
})

test_that("add_links rejects malformed list entries", {
  im <- Infomap(silent = TRUE)
  expect_error(im$add_links(list(c(1))), "2 or 3 values")
  expect_error(im$add_links(list(c(1, 2, 3, 4))), "2 or 3 values")
  expect_error(im$add_links(list(c("a", "b"))), "numeric/integer")
  expect_error(im$add_links(list(list(1, 2, "bad"))), "weight values must be numeric")
  expect_error(im$add_links(list(list(c(1, 2), 3))), "source value must be scalar")
  expect_error(im$add_links(list(list(1, c(2, 3)))), "target value must be scalar")
  expect_error(im$add_links(list(list(1, 2, c(3, 4)))), "weight value must be scalar")
})

test_that("add_multilayer_links rejects malformed matrix/data.frame input", {
  im <- Infomap(silent = TRUE)

  expect_error(
    im$add_multilayer_links(matrix(c(1, 1, 2), ncol = 3L)),
    "4 or 5 columns"
  )
  expect_error(
    im$add_multilayer_links(data.frame(a = "x", b = 1, c = 2, d = 3)),
    "numeric/integer"
  )
  expect_error(
    im$add_multilayer_links(data.frame(a = 1, b = 1, c = 2, d = 2, w = "bad")),
    "weight column must be numeric"
  )
})

test_that("add_multilayer_links rejects malformed list entries", {
  im <- Infomap(silent = TRUE)

  expect_error(im$add_multilayer_links(list(list(c(1, 1)))), "2 or 3 values")
  expect_error(
    im$add_multilayer_links(list(list(c(1, 1), c(2, 2), 1, 2))),
    "2 or 3 values"
  )
  expect_error(
    im$add_multilayer_links(list(list(c(1), c(2, 2)))),
    "node must contain 2 values"
  )
  expect_error(
    im$add_multilayer_links(list(list(c(1, 1), c(2, 2, 3)))),
    "node must contain 2 values"
  )
  expect_error(
    im$add_multilayer_links(list(list(list(c(1, 2), 1), c(2, 2)))),
    "source layer value must be scalar"
  )
  expect_error(
    im$add_multilayer_links(list(list(c("a", 1), c(2, 2)))),
    "numeric/integer"
  )
  expect_error(
    im$add_multilayer_links(list(list(c(1, 1), c(2, 2), c(1, 2)))),
    "weight value must be scalar"
  )
  expect_error(
    im$add_multilayer_links(list(list(c(1, 1), c(2, 2), "bad"))),
    "weight values must be numeric"
  )
})

test_that("add_multilayer_intra_links rejects malformed matrix/data.frame input", {
  im <- Infomap(silent = TRUE)

  expect_error(
    im$add_multilayer_intra_links(matrix(c(1, 2), ncol = 2L)),
    "3 or 4 columns"
  )
  expect_error(
    im$add_multilayer_intra_links(data.frame(layer = "x", source = 1, target = 2)),
    "numeric/integer"
  )
  expect_error(
    im$add_multilayer_intra_links(data.frame(layer = 1, source = 1, target = 2, weight = "bad")),
    "weight column must be numeric"
  )
})

test_that("add_multilayer_intra_links rejects malformed list entries", {
  im <- Infomap(silent = TRUE)

  expect_error(im$add_multilayer_intra_links(list(c(1, 2))), "3 or 4 values")
  expect_error(im$add_multilayer_intra_links(list(c(1, 2, 3, 4, 5))), "3 or 4 values")
  expect_error(
    im$add_multilayer_intra_links(list(list(c(1, 2), 2, 3))),
    "layer value must be scalar"
  )
  expect_error(im$add_multilayer_intra_links(list(c("a", 2, 3))), "numeric/integer")
  expect_error(
    im$add_multilayer_intra_links(list(list(1, 2, 3, c(1, 2)))),
    "weight value must be scalar"
  )
  expect_error(
    im$add_multilayer_intra_links(list(list(1, 2, 3, "bad"))),
    "weight values must be numeric"
  )
})

test_that("add_multilayer_inter_links rejects malformed matrix/data.frame input", {
  im <- Infomap(silent = TRUE)

  expect_error(
    im$add_multilayer_inter_links(matrix(c(1, 2), ncol = 2L)),
    "3 or 4 columns"
  )
  expect_error(
    im$add_multilayer_inter_links(data.frame(source_layer = "x", node = 1, target_layer = 2)),
    "numeric/integer"
  )
  expect_error(
    im$add_multilayer_inter_links(data.frame(source_layer = 1, node = 1, target_layer = 2, weight = "bad")),
    "weight column must be numeric"
  )
})

test_that("add_multilayer_inter_links rejects malformed list entries", {
  im <- Infomap(silent = TRUE)

  expect_error(im$add_multilayer_inter_links(list(c(1, 2))), "3 or 4 values")
  expect_error(im$add_multilayer_inter_links(list(c(1, 2, 3, 4, 5))), "3 or 4 values")
  expect_error(
    im$add_multilayer_inter_links(list(list(c(1, 2), 2, 3))),
    "source layer value must be scalar"
  )
  expect_error(im$add_multilayer_inter_links(list(c("a", 2, 3))), "numeric/integer")
  expect_error(
    im$add_multilayer_inter_links(list(list(1, 2, 3, c(1, 2)))),
    "weight value must be scalar"
  )
  expect_error(
    im$add_multilayer_inter_links(list(list(1, 2, 3, "bad"))),
    "weight values must be numeric"
  )
  expect_error(
    im$add_multilayer_inter_links(list(c(1, 1, 1))),
    "must have layer1 != layer2"
  )
})

test_that("running twice in a row does not crash", {
  im <- Infomap(silent = TRUE)
  im$add_link(1, 2)
  im$add_link(2, 3)
  im$run()
  first <- im$num_top_modules
  im$run()
  expect_equal(im$num_top_modules, first)
})
