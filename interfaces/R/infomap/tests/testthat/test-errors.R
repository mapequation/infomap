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

test_that("running twice in a row does not crash", {
  im <- Infomap(silent = TRUE)
  im$add_link(1, 2)
  im$add_link(2, 3)
  im$run()
  first <- im$num_top_modules
  im$run()
  expect_equal(im$num_top_modules, first)
})
