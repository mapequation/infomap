# R Workflows

Use this reference when the user works in R, R igraph, R Markdown, data frames, or the R6 Infomap API.

## Authority for current syntax

Do not treat this skill as the R API manual. Before giving runnable code, inspect the installed package:

```r
packageVersion("infomap")
system.file(package = "infomap")
utils::help("cluster_infomap", package = "infomap")
utils::help("cluster_infomap_multilayer", package = "infomap")
utils::help("Infomap", package = "infomap")
utils::help("infomap_options", package = "infomap")
getNamespaceExports("infomap")
args(get("cluster_infomap", asNamespace("infomap")))
args(get("infomap_options", asNamespace("infomap")))
```

For R6 details, use installed help for `Infomap` and inspect the created object when needed:

```r
im <- infomap::Infomap(silent = TRUE)
names(im)
```

Use the r-universe package page and published repository docs when internet access is available. Use source checkout files only when the user is actually working inside an Infomap repository.

## Choose the R entry point

- Prefer the high-level `infomap` package helper for data-frame or igraph workflows when installed help confirms the call shape.
- Prefer the multilayer helper when installed help confirms the user's edge-list format.
- Prefer the R6 `Infomap` API when the user needs low-level control, custom link addition, codelength, active bindings, writers, or state/multilayer methods.
- If `igraph` is attached, qualify calls as `infomap::...` or `igraph::...` to avoid ambiguity. The two packages expose different result objects and argument names.

## Generating code

- Generate code after checking installed help, `args(...)`, or exported objects.
- Keep examples small. Use one trial for smoke tests; use a meaningful trial count for research runs only after runtime is acceptable.
- Record R package version, package path, graph source, directed/weighted choice, seed, trials, non-default options, and output artifacts.
- Preserve mappings when igraph names, physical ids, layer ids, or other labels are converted.
- For state or multilayer output, state whether results are state-level or physical-node-level.

## Minimal patterns

For a quick partition, use the installed high-level helper and inspect its returned object fields. For richer results, create an `Infomap` R6 object, add/read the network with installed methods, run it, then extract codelength, modules, and node/state tables using installed help.

Avoid copying long R examples from this skill. The exact method names, argument aliases, and result fields should come from the installed package and published docs.
