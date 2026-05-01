#!/usr/bin/env Rscript
# Build a tarball, then run R CMD check --as-cran via rcmdcheck.
# Fail the run on ERROR or unexpected WARNING. NOTEs do not fail the run.
#
# A tightly-scoped allowlist suppresses the single known WARNING about
# the Infomap C++ core writing to std::cout/std::cerr and calling exit.
# That originates in the upstream optimizer and is out of scope for the
# R bindings; addressing it requires routing the core's I/O through
# Rprintf/REprintf and replacing exit() with R errors.
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

# Allowlist: pre-existing WARNING about the Infomap C++ core using
# std::cout/std::cerr/exit. Tracked in the issue queue; remove this entry
# once the core is wrapped to route I/O through R callbacks. We require
# the warning to be the "checking compiled code" one and to mention only
# these specific symbols — anything else falls through and fails.
is_allowed_warning <- function(text) {
  if (!grepl("^checking compiled code", text)) return(FALSE)
  found_lines <- grep("^\\s*Found '", strsplit(text, "\n")[[1L]], value = TRUE)
  if (length(found_lines) == 0L) return(FALSE)
  allowed <- c(
    "Found '_exit', possibly from 'exit'",
    "possibly from 'std::cerr'",
    "possibly from 'std::cout'"
  )
  all(vapply(found_lines, function(line) {
    any(vapply(allowed, function(pat) grepl(pat, line, fixed = TRUE), logical(1L)))
  }, logical(1L)))
}

# rcmdcheck::rcmdcheck() runs R CMD build first when given a directory,
# so the source tree (pkg_dir) is left untouched.
result <- rcmdcheck::rcmdcheck(
  path     = pkg_dir,
  args     = flags,
  check_dir = check_dir,
  error_on = "never",
  quiet    = FALSE
)

unexpected_warnings <- Filter(
  function(w) !is_allowed_warning(w),
  result$warnings
)

if (length(result$errors) > 0L) {
  message(sprintf("R CMD check found %d ERROR(s).", length(result$errors)))
  quit(status = 1L, save = "no")
}

if (length(unexpected_warnings) > 0L) {
  message(sprintf(
    "R CMD check found %d unexpected WARNING(s) (allowlist did not match):",
    length(unexpected_warnings)
  ))
  for (w in unexpected_warnings) message("  ", strsplit(w, "\n")[[1L]][1L])
  quit(status = 1L, save = "no")
}

allowed_count <- length(result$warnings) - length(unexpected_warnings)
message(sprintf(
  "R CMD check passed (%d allowlisted WARNING(s), %d NOTE(s)).",
  allowed_count, length(result$notes)
))
quit(status = 0L, save = "no")
