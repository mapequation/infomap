test_that("get_links returns input weights before run", {
  im <- Infomap(silent = TRUE, directed = TRUE)
  im$add_links(data.frame(
    source = c(2, 1, 3),
    target = c(3, 2, 1),
    weight = c(1.5, 2.5, 3.5)
  ))

  links <- im$get_links()
  expect_s3_class(links, "data.frame")
  expect_named(links, c("source", "target", "weight", "flow"))
  expect_equal(nrow(links), 3L)

  links <- links[order(links$source, links$target), ]
  row.names(links) <- NULL
  expect_equal(links$source, c(1L, 2L, 3L))
  expect_equal(links$target, c(2L, 3L, 1L))
  expect_equal(links$weight, c(2.5, 1.5, 3.5))
  expect_equal(links$flow, c(0, 0, 0))
})

test_that("get_links returns link flow after run", {
  im <- Infomap(silent = TRUE, directed = TRUE, num_trials = 1L)
  im$add_links(list(c(1, 2, 2.5), c(2, 3, 1.5), c(3, 1, 3.5)))
  im$run()

  links <- im$get_links()
  expect_s3_class(links, "data.frame")
  expect_named(links, c("source", "target", "weight", "flow"))
  expect_equal(nrow(links), 3L)
  expect_type(links$flow, "double")
  expect_false(anyNA(links$flow))
  expect_true(any(links$flow > 0))

  expect_equal(im$links, links)
  expect_equal(im$flow_links, links[c("source", "target", "flow")])
})

test_that("get_links returns an empty data frame for empty networks", {
  im <- Infomap(silent = TRUE)
  links <- im$get_links()

  expect_s3_class(links, "data.frame")
  expect_named(links, c("source", "target", "weight", "flow"))
  expect_equal(nrow(links), 0L)
  expect_type(links$source, "integer")
  expect_type(links$target, "integer")
  expect_type(links$weight, "double")
  expect_type(links$flow, "double")
})
