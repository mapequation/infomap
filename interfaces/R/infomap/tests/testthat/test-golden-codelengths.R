# Golden-codelength parity: the R API must reproduce the CLI-generated manifest
# (test/fixtures/expected/golden-codelengths.json) for the same network and the
# same raw CLI flag string. With the ctest freshness gate (CLI) and
# test_golden_codelengths.py (Python), this pins codelength three-way.
#
# The manifest and its networks are copied into tests/testthat/golden/ at stage
# time by scripts/stage_r_package.py (the source fixtures are not in the
# package), so the suite skips gracefully when run against the unstaged skeleton.

test_that("golden-codelength manifest reproduces across the R API", {
  skip_if_not_installed("jsonlite")
  manifest_path <- test_path("golden", "golden-codelengths.json")
  skip_if_not(file.exists(manifest_path), "golden fixtures not staged")

  entries <- jsonlite::fromJSON(manifest_path, simplifyVector = FALSE)
  for (entry in entries) {
    network <- test_path("golden", basename(entry$network))
    im <- Infomap(args = entry$flags)
    im$read_file(network)
    im$run()

    # Tolerance matches the C++ suite (checkApproxCodelength); counts are exact.
    expect_equal(
      im$codelength,
      entry$codelength,
      tolerance = 1e-10,
      info = entry$id
    )
    expect_equal(im$num_top_modules, entry$num_top_modules, info = entry$id)
    expect_equal(im$num_levels, entry$num_levels, info = entry$id)
  }
})
