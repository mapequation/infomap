# infomap

R bindings for the [Infomap](https://www.mapequation.org/infomap/) network
clustering algorithm. Infomap decomposes a network into modules by
optimally compressing a description of information flows on the network
using the Map Equation.

## Installation

Pre-built binaries (no compiler required) — the recommended path:

```r
install.packages(
  "infomap",
  repos = c("https://mapequation.r-universe.dev", "https://cloud.r-project.org")
)
```

Source install from a release tarball — pick the version from the
[GitHub releases page](https://github.com/mapequation/infomap/releases):

```r
# Replace VERSION with the desired tag, e.g. "2.9.2".
install.packages(
  paste0("https://github.com/mapequation/infomap/releases/download/v",
         "VERSION", "/infomap_", "VERSION", ".tar.gz"),
  repos = NULL
)
```

Source install from a git checkout (requires Python 3 and a C++14 compiler):

```r
remotes::install_github("mapequation/infomap", subdir = "interfaces/R/infomap")
```

## Quick start

```r
library(infomap)

im <- Infomap(silent = TRUE, num_trials = 10)
im$add_links(list(c(1, 2), c(1, 3), c(2, 3),
                  c(3, 4),
                  c(4, 5), c(4, 6), c(5, 6)))
im$run()

im$codelength
im$num_top_modules
im$modules                # named integer vector: node_id -> module_id
as.data.frame(im)         # one row per node, with path / flow / name / module_id
```

See `?Infomap` for the user-facing constructor surface, `?InfomapClass`
for the full method and active-binding reference, and `?infomap_options`
for the complete list of named options.

## Working with igraph

`igraph` is a `Suggests` dependency, not a hard requirement — install
it separately if you want to import existing graphs:

```r
install.packages("igraph")

library(igraph)
library(infomap)

g <- make_graph("Zachary")

im <- Infomap(silent = TRUE, two_level = TRUE)
mapping <- im$add_igraph(g)   # invisibly returns id -> vertex name
im$run()

comm <- im$as_communities(g)  # igraph 'communities' object
```

`add_igraph()` and `as_communities()` raise an informative error if
`igraph` is not installed.

**Node id convention.** Infomap uses 0-indexed internal node ids; igraph
uses 1-indexed `V(g)`. The `mapping` returned by `add_igraph()` recovers
the original igraph vertex names (or stringified indices) keyed by
internal id, so you can join Infomap results back to the original
graph. Module assignments returned by `im$modules` and
`as.data.frame(im)` use the 0-indexed internal ids.

## Related packages

`infomap` is also distributed as a Python package on PyPI and an npm
package as `@mapequation/infomap`. The C++ core, SWIG interface, and
documentation live in [the Infomap repository](https://github.com/mapequation/infomap).
