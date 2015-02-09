
source("load-infomap.R")

conf <- init("--two-level -v")

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

cat("Num links:", network$numLinks(), '\n')

network$finalizeAndCheckNetwork()

tree <- HierarchicalNetwork(conf)

run(network, tree);

codelength = tree$codelength()

cat("Codelength:", codelength, '\n')