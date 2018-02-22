
source("load-infomap.R")

infomap <- Infomap("--two-level")

# Set the start index for bipartite nodes
infomap$setBipartiteNodesFrom(5)
# Add weight as an optional third argument
infomap$addLink(5, 0)
infomap$addLink(5, 1)
infomap$addLink(5, 2)
infomap$addLink(6, 2)
infomap$addLink(6, 3)
infomap$addLink(6, 4)

infomap$run()

tree <- infomap$tree

clusterIndexLevel <- 1 # 1, 2, ... or -1 for top, second, ... or lowest cluster level
leafIt <- tree$leafIter(clusterIndexLevel)

cat("Partitioned network in", tree$numTopModules(), "modules with codelength", tree$codelength(), "bits:\n")
while (!leafIt$isEnd()) {
	cat("Node:", leafIt$data$name, "module:", leafIt$moduleIndex(), '\n')
	leafIt$stepForward()
}

