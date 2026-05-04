## bootstrap.R — executed by r-universe before R CMD build.
## Stages the C++ core and SWIG outputs into this package tree so that
## R CMD build bundles a self-contained source tarball.
##
## When run from a full git checkout (the only supported context), the
## package lives at interfaces/R/infomap/ and the repo root is three
## directories up.

pkg_dir   <- normalizePath(getwd())
repo_root <- normalizePath(file.path(pkg_dir, "..", "..", ".."))
script    <- file.path(repo_root, "scripts", "stage_r_package.py")

if (!file.exists(script)) {
  stop(
    "stage_r_package.py not found at: ", script, "\n",
    "bootstrap.R requires a full git checkout of mapequation/infomap.",
    call. = FALSE
  )
}

py <- Sys.which("python3")
if (!nzchar(py)) {
  py <- Sys.which("python")
}
if (!nzchar(py)) {
  stop("Neither python3 nor python found on PATH; required to run stage_r_package.py.", call. = FALSE)
}

message("Staging infomap R package in-place via stage_r_package.py ...")
ret <- system2(py, args = c(shQuote(script), "--in-place"))
if (!identical(ret, 0L)) {
  stop("stage_r_package.py --in-place failed with exit code ", ret, call. = FALSE)
}
message("Staging complete.")
