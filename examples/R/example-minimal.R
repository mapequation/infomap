# Minimal Infomap example.
#
# Run with: Rscript examples/R/example-minimal.R
# Requires the `infomap` R package installed (e.g. via `make dev-r-install`).

library(infomap)

# --- R6 API ----------------------------------------------------------------
# Build a network link by link via the Infomap R6 class.

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
  "[R6]     %d modules, codelength %.4f bits.\n",
  im$num_top_modules,
  im$codelength
))
print(im$modules)

# --- cluster_infomap helper ------------------------------------------------
# Same network expressed as a data.frame edge list. Columns are positional:
# (source, target, [weight]).

edges <- data.frame(
  source = c(0, 0, 0, 1, 1, 2, 2, 3, 3, 3, 4, 4, 5, 5),
  target = c(1, 2, 3, 0, 2, 1, 0, 0, 4, 5, 3, 5, 4, 3)
)

result <- cluster_infomap(
  edges,
  silent = TRUE,
  two_level = TRUE,
  directed = TRUE
)

cat(sprintf(
  "[helper] %d modules, codelength %.4f bits.\n",
  result$num_top_modules,
  result$codelength
))
print(result$modules)
