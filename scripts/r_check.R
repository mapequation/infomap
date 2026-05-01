#!/usr/bin/env Rscript
# Run R CMD check and exit non-zero only if there are ERRORs (not warnings).
# Usage: Rscript scripts/r_check.R <pkg-dir> <check-outdir> [extra R CMD check flags...]

args <- commandArgs(trailingOnly = TRUE)
if (length(args) < 2L) {
  stop("Usage: r_check.R <pkg-dir> <check-outdir> [flags...]", call. = FALSE)
}
pkg_dir   <- args[1L]
check_dir <- args[2L]
flags     <- if (length(args) > 2L) args[3L:length(args)] else character(0L)

cmd_args <- c("check", flags, sprintf("--output=%s", check_dir), pkg_dir)
ret      <- system2("R", c("CMD", cmd_args))

log_file <- file.path(check_dir,
  paste0(basename(pkg_dir), ".Rcheck"),
  "00check.log")

if (!file.exists(log_file)) {
  # R CMD check itself failed (e.g. could not install).
  quit(status = ret, save = "no")
}

log <- readLines(log_file)
status_line <- grep("^Status:", log, value = TRUE)

has_error <- length(status_line) > 0L &&
  grepl("ERROR", status_line, fixed = TRUE)

if (has_error) {
  message(status_line)
  quit(status = 1L, save = "no")
}
message(if (length(status_line) > 0L) status_line else "Status: OK")
quit(status = 0L, save = "no")
