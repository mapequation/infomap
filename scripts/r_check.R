#!/usr/bin/env Rscript
# Build a tarball, then run R CMD check --as-cran via rcmdcheck.
# Fail on any ERROR or WARNING. NOTEs do not fail the run.
#
# Usage: Rscript scripts/r_check.R <pkg-dir> <check-outdir> [extra R CMD check flags...]

args <- commandArgs(trailingOnly = TRUE)
if (length(args) < 2L) {
  stop("Usage: r_check.R <pkg-dir> <check-outdir> [flags...]", call. = FALSE)
}
pkg_dir   <- normalizePath(args[1L], mustWork = TRUE)
check_dir <- args[2L]
flags     <- if (length(args) > 2L) args[3L:length(args)] else character(0L)

if (!requireNamespace("rcmdcheck", quietly = TRUE)) {
  stop(
    "rcmdcheck is required. Install it with install.packages('rcmdcheck').",
    call. = FALSE
  )
}

dir.create(check_dir, showWarnings = FALSE, recursive = TRUE)

# Strip parent-make jobserver tokens so the child `make` invoked by
# R CMD INSTALL doesn't inherit a broken jobserver pipe (manifests as
# "/bin/sh: Bad file descriptor"). The parent `make` already provides
# parallelism at the higher level.
Sys.unsetenv("MAKEFLAGS")
Sys.unsetenv("MFLAGS")

# rcmdcheck::rcmdcheck() runs R CMD build first when given a directory,
# so the source tree (pkg_dir) is left untouched.
result <- rcmdcheck::rcmdcheck(
  path     = pkg_dir,
  args     = flags,
  check_dir = check_dir,
  error_on = "warning",
  quiet    = FALSE
)

# rcmdcheck's error_on triggers a non-zero exit on the chosen severity,
# so reaching here means at most NOTEs.
if (length(result$notes) > 0L) {
  message(sprintf("R CMD check passed with %d NOTE(s).", length(result$notes)))
}
quit(status = 0L, save = "no")
