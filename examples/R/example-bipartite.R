
source("load-infomap.R")

infomap <- Infomap("--two-level")

# Set the start index for the feature nodes
infomap$setBipartiteNodesFrom(5)

features <- c(5,5,5,6,6,6)
nodes <- c(0,1,2,2,3,4)
weights <- c(1,1,1,1,1,1)

edgelist <- cbind(features, nodes, weights)

apply(edgelist, 1, function(e) infomap$addLink(e[1], e[2], e[3]))

infomap$run()

tree <- infomap$tree

clusterIndexLevel <- 1 # 1, 2, ... or -1 for top, second, ... or lowest cluster level
leafIt <- tree$leafIter(clusterIndexLevel)

cat("Partitioned network in", tree$numTopModules(), "modules with codelength", tree$codelength(), "bits:\n")
while (!leafIt$isEnd()) {
	cat("Node:", leafIt$originalLeafIndex, "module:", leafIt$moduleIndex(), '\n')
	leafIt$stepForward()
}

