# Infomap on Zachary's karate club via igraph.
#
# Run with: Rscript examples/R/example-igraph.R
# Requires the `infomap` and `igraph` R packages.

library(igraph)
library(infomap)

g <- make_graph("Zachary")

im <- Infomap(silent = TRUE, two_level = TRUE)
im$add_igraph(g)
im$run()

cat(sprintf(
  "Partitioned %d-node network in %d modules with codelength %.4f bits.\n",
  im$num_nodes,
  im$num_top_modules,
  im$codelength
))

comm <- im$as_communities(g)
print(comm)

# Uncomment to plot:
# plot(comm, g)
