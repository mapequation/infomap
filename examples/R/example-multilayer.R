# Multilayer Infomap example.
#
# Run with: Rscript examples/R/example-multilayer.R
# Requires the `infomap` R package installed (e.g. via `make dev-r-install`).
#
# A small two-layer network: two intra-layer triangles on physical nodes
# {1, 2, 3}, plus inter-layer couplings between matching physical nodes.

library(infomap)

# --- cluster_infomap_multilayer: intra-layer-only edge list ---------------
# When all links are within layers, Infomap couples layers automatically
# via `multilayer_relax_rate` (default 0.15).

intra <- data.frame(
  layer     = c(1, 1, 1, 2, 2, 2),
  node_from = c(1, 2, 3, 1, 2, 3),
  node_to   = c(2, 3, 1, 2, 3, 1)
)

result_intra <- cluster_infomap_multilayer(
  intra,
  silent = TRUE, num_trials = 5, two_level = TRUE
)

cat(sprintf(
  "[intra]  %d modules across %d layers, codelength %.4f bits.\n",
  result_intra$num_top_modules,
  length(unique(result_intra$nodes$layer_id)),
  result_intra$codelength
))
print(result_intra$nodes)

# --- cluster_infomap_multilayer: full multilayer edge list ----------------
# Mix intra-layer links (same `layer_from`/`layer_to`) and inter-layer
# couplings (same `node_from`/`node_to` across layers) in one data.frame.

edges <- data.frame(
  layer_from = c(1, 1, 1, 2, 2, 2, 1, 1, 1),
  node_from  = c(1, 2, 3, 1, 2, 3, 1, 2, 3),
  layer_to   = c(1, 1, 1, 2, 2, 2, 2, 2, 2),
  node_to    = c(2, 3, 1, 2, 3, 1, 1, 2, 3),
  weight     = c(1, 1, 1, 1, 1, 1, 0.5, 0.5, 0.5)
)

result_full <- cluster_infomap_multilayer(
  edges,
  silent = TRUE, num_trials = 5, two_level = TRUE
)

cat(sprintf(
  "[full]   %d modules, codelength %.4f bits.\n",
  result_full$num_top_modules, result_full$codelength
))
print(result_full$nodes)

# --- R6 API: equivalent intra + explicit inter-layer couplings ------------
# The lower-level path lets you call add_multilayer_intra_link() and
# add_multilayer_inter_link() separately. Use this when intra and inter
# couplings come from different sources or carry different semantics.

im <- Infomap(silent = TRUE, num_trials = 5, two_level = TRUE)

for (layer in 1:2) {
  for (e in list(c(1, 2), c(2, 3), c(3, 1))) {
    im$add_multilayer_intra_link(layer, e[1], e[2], 1.0)
  }
}
for (n in 1:3) {
  im$add_multilayer_inter_link(1, n, 2, 0.5)
}

im$run()

cat(sprintf(
  "[R6]     %d modules, codelength %.4f bits.\n",
  im$num_top_modules, im$codelength
))
print(as.data.frame(im))
