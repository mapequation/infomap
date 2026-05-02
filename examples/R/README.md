# Infomap R examples

Each script is a standalone runnable file that depends on the
[`infomap`](../../interfaces/R/infomap/) R package.

## Install the package

The recommended way is to use a pre-built binary:

```r
install.packages(
  "infomap",
  repos = c("https://mapequation.r-universe.dev", "https://cloud.r-project.org")
)
```

To build and install from the working tree (requires Python 3 and a C++14
compiler; SWIG 4.4.1 only when refreshing tracked SWIG outputs):

```sh
make dev-r-install
```

## Run the examples

```sh
Rscript examples/R/example-minimal.R
Rscript examples/R/example-igraph.R   # also requires `igraph`
```

`example-minimal.R` constructs a small directed network programmatically and
prints the resulting partition. `example-igraph.R` shows how to import an
existing `igraph` graph and convert the result back into an
`igraph::communities` object.
