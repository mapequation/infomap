#' infomap: R bindings for the Infomap network clustering algorithm
#'
#' Infomap decomposes a network into modules by optimally compressing a
#' description of information flows on the network using the Map
#' Equation. This package provides an idiomatic R6 facade over the
#' SWIG-generated bindings.
#'
#' @section Main entry points:
#' \itemize{
#'   \item [Infomap()] — the user-facing R6 class.
#'   \item [infomap_options()] — build a reusable options list.
#'   \item [as.data.frame.Infomap()] — tidy node-level data.frame.
#'   \item [entropy()], [perplexity()], [plogp()] — information-theoretic helpers.
#' }
#'
#' @keywords internal
#' @useDynLib infomap
#' @importFrom R6 R6Class
#' @importFrom methods new slot as is callNextMethod cacheMetaData extends setClass
"_PACKAGE"
