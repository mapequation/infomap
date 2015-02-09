
source("load-infomap.R")

conf <- init("--two-level --silent")

network <- Network(conf);

network$addLink(0, 1);
network$addLink(0, 2);
network$addLink(0, 3);
network$addLink(1, 0);
network$addLink(1, 2);
network$addLink(2, 1);
network$addLink(2, 0);
network$addLink(3, 0);
network$addLink(3, 4);
network$addLink(3, 5);
network$addLink(4, 3);
network$addLink(4, 5);
network$addLink(5, 4);
network$addLink(5, 3);

network$finalizeAndCheckNetwork()

cat("Created network with", network$numNodes(), "nodes and", network$numLinks(), "links.\n")

tree <- HierarchicalNetwork(conf)

run(network, tree);

leafIt <- tree$leafIter()

cat("Partitioned network in", tree$getRootNode()$childDegree(), "modules with codelength", tree$codelength(), "bits:\n")
while (!leafIt$isEnd()) {
	cat("Node:", leafIt$base()$data$name, "module:", leafIt$base()$parentNode$parentIndex, '\n')
	leafIt$stepForward()
}

