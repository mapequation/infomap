#' Coerce Infomap results to a data.frame
#'
#' Returns one row per leaf node in the partitioned tree. Mirrors
#' Python's `Infomap.get_dataframe()`.
#'
#' @param x An `Infomap` instance after `run()` has been called.
#' @param row.names Standard `as.data.frame` argument; ignored.
#' @param optional Standard `as.data.frame` argument; ignored.
#' @param states If `TRUE`, return one row per state node (for
#'   higher-order networks); otherwise merge state nodes with the same
#'   physical id within a module. Default `TRUE`.
#' @param level Level of the hierarchy used for the `module_id` column. `1`
#'   gives top-level modules, `-1` the bottom level. Default `1`.
#' @param tibble If `TRUE`, return a `tibble` (requires the `tibble`
#'   package). Default `FALSE` returns a plain `data.frame` so the
#'   return type is independent of installed packages.
#' @param ... Unused.
#' @param depth_level Deprecated spelling of `level`; leaves the infomap R
#'   surface in 3.0.
#'
#' @return A `data.frame` (or a `tibble` when `tibble = TRUE`) with
#'   columns `state_id`, `node_id`, `module_id`, `flow`, `name`,
#'   (for multilayer networks) `layer_id` and (for higher-order
#'   networks) `state_name`. The `name` column always carries the
#'   physical node name; `state_name` carries the per-state-node name,
#'   falling back to the physical name when a state node is unnamed.
#'
#' @examples
#' im <- Infomap(silent = TRUE)
#' im$add_links(list(c(1, 2), c(2, 3), c(3, 1), c(3, 4), c(4, 5), c(5, 6), c(6, 4)))
#' im$run()
#' as.data.frame(im)
#' @export
as.data.frame.Infomap <- function(
  x,
  row.names = NULL,
  optional = FALSE,
  states = TRUE,
  level = 1L,
  tibble = FALSE,
  ...,
  depth_level = NULL
) {
  level <- .resolve_level(level, depth_level, "as.data.frame")
  raw <- x$get_nodes(
    level = level,
    states = isTRUE(states)
  )
  df <- data.frame(
    state_id = raw$state_id,
    node_id = raw$node_id,
    module_id = raw$module_id,
    flow = raw$flow,
    stringsAsFactors = FALSE
  )
  if (!is.null(raw$layer_id)) {
    df$layer_id <- raw$layer_id
  }
  df$name <- vapply(
    raw$node_id,
    function(id) {
      nm <- x$get_name(id, default = NA_character_)
      if (is.null(nm) || is.na(nm)) NA_character_ else as.character(nm)
    },
    character(1L)
  )

  # For higher-order networks, expose the per-state-node name alongside the
  # physical `name`. Mirrors the conditional `layer_id` column above; omitted
  # for first-order networks where it would just duplicate `name`.
  if (isTRUE(x$have_memory)) {
    state_name <- vapply(
      raw$state_id,
      function(id) {
        nm <- x$get_state_name(id, default = NA_character_)
        if (is.null(nm) || is.na(nm)) NA_character_ else as.character(nm)
      },
      character(1L)
    )
    # Fall back to the physical name where a state node has no name.
    missing <- is.na(state_name)
    state_name[missing] <- df$name[missing]
    df$state_name <- state_name
  }

  if (isTRUE(tibble)) {
    if (!requireNamespace("tibble", quietly = TRUE)) {
      stop(
        "tibble = TRUE requires the 'tibble' package. ",
        "Install it with install.packages(\"tibble\").",
        call. = FALSE
      )
    }
    return(tibble::as_tibble(df))
  }
  df
}
