# Run the differential corpus (cases.tsv) through the installed R package.
#
#   Rscript test/differential/run_r.R <out-dir>
#
# Run from the repository root. Writes the layout differential.py compares:
# <id>.tree and <id>.codelength per case, or <id>.error.
library(infomap)

args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L) {
  stop("usage: run_r.R <out-dir>")
}
out_dir <- args[[1L]]
dir.create(out_dir, recursive = TRUE, showWarnings = FALSE)

lines <- readLines("test/differential/cases.tsv")
lines <- lines[nzchar(lines) & !startsWith(lines, "#")]
stopifnot(identical(lines[[1L]], "id\tnetwork\toptions"))

option_value <- function(value) {
  if (is.na(value)) {
    return(TRUE)
  }
  number <- suppressWarnings(as.numeric(value))
  if (is.na(number)) {
    return(value)
  }
  if (grepl("^-?[0-9]+$", value)) as.integer(value) else number
}

for (line in lines[-1L]) {
  fields <- strsplit(line, "\t", fixed = TRUE)[[1L]]
  id <- fields[[1L]]
  network <- fields[[2L]]
  tokens <- if (length(fields) > 2L) {
    strsplit(fields[[3L]], " ", fixed = TRUE)[[1L]]
  } else {
    character()
  }
  opts <- list(seed = 123L, silent = TRUE)
  for (token in tokens[nzchar(tokens)]) {
    parts <- strsplit(token, "=", fixed = TRUE)[[1L]]
    opts[[parts[[1L]]]] <- option_value(parts[2L])
  }
  outcome <- tryCatch(
    {
      im <- do.call(Infomap, opts)
      im$read_file(network)
      im$run()
      im$write_tree(
        file.path(out_dir, paste0(id, ".tree")),
        states = im$have_memory
      )
      writeLines(
        sprintf("%.17g", im$codelength),
        file.path(out_dir, paste0(id, ".codelength"))
      )
      NULL
    },
    error = function(e) conditionMessage(e)
  )
  if (!is.null(outcome)) {
    writeLines(outcome, file.path(out_dir, paste0(id, ".error")))
  }
}
