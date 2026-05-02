test_that("write_tree round-trips through read_file", {
  im <- Infomap(silent = TRUE, num_trials = 3)
  im$add_links(list(c(1, 2), c(1, 3), c(2, 3),
                    c(3, 4),
                    c(4, 5), c(4, 6), c(5, 6)))
  im$run()
  expected_modules <- im$num_top_modules

  tmp <- tempfile(fileext = ".tree")
  on.exit(unlink(tmp), add = TRUE)
  im$write_tree(tmp)
  expect_true(file.exists(tmp))
  expect_gt(file.size(tmp), 0L)

  im2 <- Infomap(silent = TRUE, no_infomap = TRUE, cluster_data = tmp)
  im2$add_links(list(c(1, 2), c(1, 3), c(2, 3),
                     c(3, 4),
                     c(4, 5), c(4, 6), c(5, 6)))
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
