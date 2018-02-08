
source("load-infomap.R")

infomap <- Infomap("--two-level --silent")
# Add output directory (and output name) to automatically write result to file
# infomap <- Infomap("--two-level . --out-name test")

infomap$addLink(0, 1);
infomap$addLink(0, 2);
infomap$addLink(0, 3);
infomap$addLink(1, 0);
infomap$addLink(1, 2);
infomap$addLink(2, 1);
infomap$addLink(2, 0);
infomap$addLink(3, 0);
infomap$addLink(3, 4);
infomap$addLink(3, 5);
infomap$addLink(4, 3);
infomap$addLink(4, 5);
infomap$addLink(5, 4);
infomap$addLink(5, 3);

infomap$run()

tree <- infomap$tree

clusterIndexLevel <- 1 # 1, 2, ... or -1 for top, second, ... or lowest cluster level
leafIt <- tree$leafIter(clusterIndexLevel)

cat("Partitioned network in", tree$numTopModules(), "modules with codelength", tree$codelength(), "bits:\n")
while (!leafIt$isEnd()) {
	cat("Node:", leafIt$data$name, "module:", leafIt$moduleIndex(), '\n')
	leafIt$stepForward()
}
