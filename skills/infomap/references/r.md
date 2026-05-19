# R Workflows

Use this reference when the user works in R, R igraph, R Markdown, data frames, or the R6 Infomap API.

## Sources to inspect

- Installed R help: `?infomap::cluster_infomap`, `?infomap::Infomap`, and `?infomap::infomap_options`.
- Installed package metadata, such as `packageVersion("infomap")`.
- The r-universe package page and published repository docs when internet access is available.
- Source checkout files only when the user is actually working inside an Infomap repository.

## Choose the R entry point

- Use `cluster_infomap()` for a one-call data-frame workflow that returns an `infomap_result`.
- Use `cluster_infomap_multilayer()` for multilayer edge-list data frames.
- Use the R6 `Infomap(...)` API when the user needs low-level control, custom link addition, codelength, active bindings, or state/multilayer methods.
- If `igraph` is attached, qualify calls as `infomap::cluster_infomap()` or `igraph::cluster_infomap()` to avoid ambiguity.

## Common patterns

For examples in answers, `num_trials = 20` is a reasonable research default. For actual validation runs, use a tiny graph or `num_trials = 1` first. Ask before running large graphs, many trials, repeated seeds, multilayer sweeps, or expensive R Markdown/notebook-style workflows.

Data-frame workflow:

```r
library(infomap)

edges <- data.frame(
  source = c(1, 1, 2, 3, 4, 4, 5),
  target = c(2, 3, 3, 4, 5, 6, 6)
)

result <- cluster_infomap(
  edges,
  silent = TRUE,
  num_trials = 20,
  seed = 123
)

result$codelength
result$num_top_modules
result$modules
result$nodes
```

Low-level R6 workflow:

```r
library(infomap)

im <- Infomap(silent = TRUE, num_trials = 20, seed = 123)
im$add_links(list(c(1, 2), c(1, 3), c(2, 3), c(3, 4)))
im$run()

im$codelength
im$num_top_modules
as.data.frame(im)
```

igraph workflow:

```r
library(igraph)
library(infomap)

g <- make_graph("Zachary")

im <- Infomap(silent = TRUE, two_level = TRUE, num_trials = 20, seed = 123)
mapping <- im$add_igraph(g)
im$run()

communities <- im$as_communities(g)
```

Namespace choice when both packages are attached:

```r
# Infomap R package helper: returns an infomap_result with nodes/modules tables.
result <- infomap::cluster_infomap(g, silent = TRUE, num_trials = 20, seed = 123)

# igraph helper: returns an igraph communities object.
communities <- igraph::cluster_infomap(g, nb.trials = 20)
```

## Multilayer reminders

- Use `cluster_infomap_multilayer()` when multilayer links are already in a data frame.
- Intra-layer-only edge lists can rely on multilayer coupling parameters when appropriate.
- Use R6 methods such as `add_multilayer_intra_link()` and `add_multilayer_inter_link()` when intra- and inter-layer links come from different sources or carry different meanings.

## Result and reporting reminders

- `result$modules` is a named integer vector from node id to module id.
- `result$nodes` or `as.data.frame(im)` gives one row per node/state with fields such as flow, name, module id, and layer/state ids when available.
- State and multilayer graphs can use internal integer ids; preserve the returned mapping when labels matter.
- Record R package version, graph source, directed/weighted choice, seed, `num_trials`, and non-default options.
