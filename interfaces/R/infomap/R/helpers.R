# Internal helpers.

#' @noRd
plogp <- function(p) {
  out <- numeric(length(p))
  pos <- !is.na(p) & p > 0
  out[pos] <- p[pos] * log2(p[pos])
  out
}

#' @noRd
entropy <- function(p) {
  -sum(plogp(p))
}

#' @noRd
perplexity <- function(p) {
  2 ^ entropy(p)
}

# Top-level helpers exported alongside the Infomap class.

#' Multilayer node helper
#'
#' Convenience constructor for `c(layer_id, node_id)` integer pairs
#' passed to multilayer-link methods.
#'
#' @param layer_id Integer layer id.
#' @param node_id Integer node id.
#' @return A length-2 integer vector with class `"multilayer_node"`.
#' @examples
#' src <- multilayer_node(1L, 2L)
#' tgt <- multilayer_node(1L, 3L)
#' @export
multilayer_node <- function(layer_id, node_id) {
  out <- c(as.integer(layer_id), as.integer(node_id))
  names(out) <- c("layer_id", "node_id")
  class(out) <- c("multilayer_node", class(out))
  out
}

#' Run Infomap from the command line
#'
#' Parses arguments from `commandArgs()` (or the `args` parameter), runs
#' Infomap, and exits. Useful for invoking the optimizer from `Rscript`:
#' `Rscript -e 'infomap::main()' --args <flags...>`.
#'
#' Each element of `args` is treated as a single token; spaces inside a
#' token (e.g. in filenames) survive via `shQuote`. Pass tokens, not a
#' single pre-joined string.
#'
#' @param args Character vector of CLI arguments (default: from `commandArgs()`).
#' @return Invisibly returns the resulting `Infomap` instance.
#' @export
main <- function(args = NULL) {
  if (is.null(args)) args <- commandArgs(trailingOnly = TRUE)
  args <- as.character(args)
  cli <- paste(shQuote(args), collapse = " ")
  im <- Infomap(args = cli)
  im$run()
  invisible(im)
}
