# Minimal Infomap example.
#
# Run with: Rscript examples/R/example-minimal.R
# Requires the `infomap` R package installed (e.g. via `make dev-r-install`).

library(infomap)

im <- Infomap(silent = TRUE, two_level = TRUE, directed = TRUE)

im$add_link(0, 1)
im$add_link(0, 2)
im$add_link(0, 3)
im$add_link(1, 0)
im$add_link(1, 2)
im$add_link(2, 1)
im$add_link(2, 0)
im$add_link(3, 0)
im$add_link(3, 4)
im$add_link(3, 5)
im$add_link(4, 3)
im$add_link(4, 5)
im$add_link(5, 4)
im$add_link(5, 3)

im$run()

cat(sprintf(
  "Partitioned network in %d modules with codelength %.4f bits.\n",
  im$num_top_modules, im$codelength
))

print(im$modules)
