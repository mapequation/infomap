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

See `?Infomap` for the full method reference and `?infomap_options` for
the complete list of named options.

## Related packages

`infomap` is also distributed as a Python package on PyPI and an npm
package as `@mapequation/infomap`. The C++ core, SWIG interface, and
documentation live in [the Infomap repository](https://github.com/mapequation/infomap).
