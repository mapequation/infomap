#include "Config.h"
#include "version.h"
#include <vector>
#include "ProgramInterface.h"
#include "SafeFile.h"
#include "../utils/exceptions.h"
#include "../utils/FileURI.h"
#include "../utils/Log.h"

namespace infomap {

const std::string FlowModel::undirected = "undirected";
const std::string FlowModel::directed = "directed";
const std::string FlowModel::undirdir = "undirdir";
const std::string FlowModel::outdirdir = "outdirdir";
const std::string FlowModel::rawdir = "rawdir";

Config Config::fromString(std::string flags, bool requireFileInput)
{
    Config conf;

    ProgramInterface api("Infomap",
			"Implementation of the Infomap clustering algorithm based on the Map Equation (see www.mapequation.org)",
			INFOMAP_VERSION);

	api.setGroups({"Input", "Accuracy", "Output"});

	std::vector<std::string> optionalOutputDir; // Used if !requireFileInput
	// --------------------- Input options ---------------------
	if (requireFileInput)
	{
		api.addNonOptionArgument(conf.networkFile, "network_file",
			"The file containing the network data. Assumes a link list format if no Pajek formatted heading.", "Input");

		// api.addOptionalNonOptionArguments(conf.additionalInput, "[additional input]",
		// 		"More network layers for multilayer.", true);
	}
	else
	{
		api.addOptionArgument(conf.networkFile, "input",
			"The file containing the network data. Assumes a link list format if no Pajek formatted heading.", "p", "Input");
	}

	api.addOptionArgument(conf.inputFormat, 'i', "input-format",
			"Specify input format ('pajek', 'link-list', 'states', 'path', 'multilayer' or 'bipartite') to override automatically recognized type.", "s", "Input", true);

	// api.addOptionArgument(conf.withMemory, "with-memory",
	// 		"Use second order Markov dynamics and let nodes be part of different modules. Simulate memory from first-order data if not '3gram' input.", true);

//	api.addOptionArgument(conf.bipartite, "bipartite",
//			"Let the source id of a link belong to a different kind of nodes and ignore that set in the output.");

	api.addOptionArgument(conf.skipAdjustBipartiteFlow, "skip-adjust-bipartite-flow",
			"Skip distributing all flow from the bipartite nodes (first column) to the ordinary nodes (second column).", "Input", true);

	// api.addOptionArgument(conf.withMemory, "overlapping",
	// 		"Let nodes be part of different and overlapping modules. Applies to ordinary networks by first representing the memoryless dynamics with memory nodes.", "Input");

	api.addOptionArgument(conf.weightThreshold, "weight-threshold",
			"Limit the number of links to read from the network. Ignore links with less weight than the threshold. (Default: 0)", "f", "Input", true);

	api.addOptionArgument(conf.unweightedPaths, "unweighted-paths",
			"Assume last value in a path is a node instead of default a weight of the path.", "Input", true);

	api.addOptionArgument(conf.pathMarkovOrder, "path-markov-order",
			"Markov order of the network generated from paths.", "n", "Input", true);

	// api.addOptionArgument(conf.nonBacktracking, "non-backtracking",
	// 		"Use non-backtracking dynamics and let nodes be part of different and overlapping modules. Applies to ordinary networks by first representing the non-backtracking dynamics with memory nodes.", "Input", true);

	api.addOptionArgument(conf.includeSelfLinks, 'k', "include-self-links",
			"Include links with the same source and target node. (Ignored by default.)", "Input", true);

	api.addOptionArgument(conf.skipCompleteDanglingMemoryNodes, "skip-complete-dangling-memory-nodes",
			"Skip add first order links to complete dangling memory nodes.", "Input", true);

	api.addOptionArgument(conf.nodeLimit, 'O', "node-limit",
			"Limit the number of nodes to read from the network. Ignore links connected to ignored nodes.", "n", "Input", true);

	api.addOptionArgument(conf.preClusterMultilayer, "pre-cluster-multilayer",
			"Pre-cluster multilayer networks layer by layer.", "Input", true);

	api.addOptionArgument(conf.clusterDataFile, 'c', "cluster-data",
			"Provide an initial two-level (.clu format) or multi-layer (.tree format) solution.", "p", "Input");

	api.addOptionArgument(conf.setUnidentifiedNodesToClosestModule, "set-unidentified-nodes-to-closest-module",
			"Merge unidentified nodes in cluster data to closest existing modules if possible.", "Input", true);

	api.addOptionArgument(conf.metaDataFile, "meta-data",
			"Provide meta data (.clu format) that should be encoded.", "p", "Input", true);

	api.addOptionArgument(conf.metaDataRate, "meta-data-rate",
			"The encoding rate of metadata. Default 1.0 means in each step.", "f", "Input", true);

	api.addOptionArgument(conf.unweightedMetaData, "unweighted-meta-data",
			"Don't weight meta data by node flow.", "Input", true);

	api.addOptionArgument(conf.noInfomap, "no-infomap",
			"Don't run Infomap. Useful to calculate codelength of provided cluster data or to print non-modular statistics.", "Input");

	// --------------------- Output options ---------------------

	api.addOptionArgument(conf.outName, "out-name",
			"Use this name for the output files, like [output_directory]/[out-name].tree", "s", "Output", true);

	api.addOptionArgument(conf.noFileOutput, '0', "no-file-output",
			"Don't print any output to file.", "Output", true);

	api.addOptionArgument(conf.printMap, "map",
			"Print the top two-level modular network in the .map format.", "Output", true);

	api.addOptionArgument(conf.printClu, "clu",
			"Print a .clu file with the top cluster ids for each node.", "Output");

	api.addOptionArgument(conf.printTree, "tree",
			"Print a .tree file with the modular hierarchy. (True by default)", "Output", true);

	api.addOptionArgument(conf.printFlowTree, "ftree",
			"Print a .ftree file with the modular hierarchy including aggregated links between (nested) modules. (Used by Network Navigator)", "Output", true);

	// api.addOptionArgument(conf.printBinaryTree, "btree",
	// 		"Print the tree in a streamable binary format.", "Output", true);

	// api.addOptionArgument(conf.printBinaryFlowTree, "bftree",
	// 		"Print the tree including horizontal flow links in a streamable binary format.", "Output", true);

	api.addOptionArgument(conf.printNodeRanks, "print-node-ranks",
			"Print the calculated flow for each node to a file.", "Output", true);

	api.addOptionArgument(conf.printFlowNetwork, "print-flow-network",
			"Print the network with calculated flow values.", "Output", true);

	api.addOptionArgument(conf.printPajekNetwork, "print-network",
			"Print the parsed network in Pajek format.", "Output", true);

	api.addOptionArgument(conf.printStateNetwork, "print-state-network",
			"Print the internal state network.", "Output", true);

	// api.addOptionArgument(conf.printExpanded, "expanded",
	// 		"Print the expanded network of memory nodes if possible.", "Output", true);

	// --------------------- Core algorithm options ---------------------
	api.addOptionArgument(conf.twoLevel, '2', "two-level",
			"Optimize a two-level partition of the network.", "Algorithm");

	api.addOptionArgument(conf.flowModel, 'f', "flow-model",
			"Specify flow model ('undirected', 'undirdir', 'directed', 'outdirdir', 'rawdir')", "s", "Algorithm");

	// bool dummyUndirected;
	// api.addOptionArgument(dummyUndirected, 'u', "undirected",
	// 		"Assume undirected links. (default)", "Algorithm");

	api.addOptionArgument(conf.directed, 'd', "directed",
			"Assume directed links.", "Algorithm");

	api.addOptionArgument(conf.undirdir, 't', "undirdir",
			"Two-mode dynamics: Assume undirected links for calculating flow, but directed when minimizing codelength.", "Algorithm", true);

	api.addOptionArgument(conf.outdirdir, "outdirdir",
			"Two-mode dynamics: Count only ingoing links when calculating the flow, but all when minimizing codelength.", "Algorithm", true);

	api.addOptionArgument(conf.rawdir, 'w', "rawdir",
			"Two-mode dynamics: Assume directed links and let the raw link weights be the flow.", "Algorithm", true);

	api.addOptionArgument(conf.recordedTeleportation, 'e', "recorded-teleportation",
			"If teleportation is used to calculate the flow, also record it when minimizing codelength.", "Algorithm", true);

	api.addOptionArgument(conf.teleportToNodes, 'o', "to-nodes",
			"Teleport to nodes instead of to links, assuming uniform node weights if no such input data.", "Algorithm", true);

	api.addOptionArgument(conf.teleportationProbability, 'p', "teleportation-probability",
			"The probability of teleporting to a random node or link.", "f", "Algorithm", true);

	// api.addOptionArgument(conf.selfTeleportationProbability, 'y', "self-link-teleportation-probability",
	// 		"Additional probability of teleporting to itself. Effectively increasing the code rate, generating more and smaller modules.", "f", "Algorithm", true);

	api.addOptionArgument(conf.markovTime, "markov-time",
			"Scale link flow with this value to change the cost of moving between modules. Higher for less modules.", "f", "Algorithm", true);

	api.addOptionArgument(conf.preferredNumberOfModules, "preferred-number-of-modules",
			"Optimize for the preferred number of modules by penalizing solutions the more they differ from the preferred number.", "n", "Algorithm", true);

	api.addOptionArgument(conf.multilayerRelaxRate, "multilayer-relax-rate",
			"The probability to relax the constraint to move only in the current layer. If negative, the inter-links have to be provided.", "f", "Algorithm", true);

	api.addOptionArgument(conf.multilayerRelaxLimit, "multilayer-relax-limit",
			"The number of neighboring layers in each direction to relax to. If negative, relax to any layer.", "n", "Algorithm", true);

	// --------------------- Performance and accuracy options ---------------------
	api.addOptionArgument(conf.seedToRandomNumberGenerator, 's', "seed",
			"A seed (integer) to the random number generator for reproducible results.", "Accuracy", "n");

	api.addOptionArgument(conf.numTrials, 'N', "num-trials",
			"The number of outer-most loops to run before picking the best solution.", "Accuracy", "n");

	api.addOptionArgument(conf.minimumCodelengthImprovement, 'm', "min-improvement",
			"Minimum codelength threshold for accepting a new solution.", "f", "Accuracy", true);

	api.addOptionArgument(conf.randomizeCoreLoopLimit, 'a', "random-loop-limit",
			"Randomize the core loop limit from 1 to 'core-loop-limit'", "Accuracy", true);

	api.addOptionArgument(conf.coreLoopLimit, 'M', "core-loop-limit",
			"Limit the number of loops that tries to move each node into the best possible module", "n", "Accuracy", true);

	api.addOptionArgument(conf.levelAggregationLimit, 'L', "core-level-limit",
			"Limit the number of times the core loops are reapplied on existing modular network to search bigger structures.", "n", "Accuracy", true);

	api.addOptionArgument(conf.tuneIterationLimit, 'T', "tune-iteration-limit",
			"Limit the number of main iterations in the two-level partition algorithm. 0 means no limit.", "n", "Accuracy", true);

	api.addOptionArgument(conf.minimumRelativeTuneIterationImprovement, 'U', "tune-iteration-threshold",
			"Set a codelength improvement threshold of each new tune iteration to 'f' times the initial two-level codelength.", "f", "Accuracy", true);

	api.addOptionArgument(conf.fastFirstIteration, "fast-first-iteration",
			"Move nodes to strongest connected module in the first iteration instead of minimizing the map equation.", "Accuracy", true);

	api.addOptionArgument(conf.fastCoarseTunePartition, 'C', "fast-coarse-tune",
			"Try to find the quickest partition of each module when creating sub-modules for the coarse-tune part.", "Accuracy", true);

	// api.addOptionArgument(conf.alternateCoarseTuneLevel, 'A', "alternate-coarse-tune-level",
	// 		"Try to find different levels of sub-modules to move in the coarse-tune part.", "Accuracy", true);

	// api.addOptionArgument(conf.coarseTuneLevel, 'S', "coarse-tune-level",
	// 		"Set the recursion limit when searching for sub-modules. A level of 1 will find sub-sub-modules.", "n", "Accuracy", true);

	api.addIncrementalOptionArgument(conf.fastHierarchicalSolution, 'F', "fast-hierarchical-solution",
			"Find top modules fast. Use -FF to keep all fast levels. Use -FFF to skip recursive part.", "Accuracy");

	api.addOptionArgument(conf.skipReplaceToOneModuleIfBetter, "skip-replace-to-one-module",
			"Keep modular solutions even if not better than all nodes in one module", "Accuracy", true);

	// api.addIncrementalOptionArgument(conf.lowMemoryPriority, 'l', "low-memory",
	// 		"Prioritize memory efficient algorithms before fast. Use -ll to optimize even more, but this may give approximate results.");

	api.addOptionArgument(conf.innerParallelization, "inner-parallelization",
			"Parallelize the innermost loop for greater speed. Note that this may give some accuracy tradeoff.", "Accuracy");

	// --------------------- Output options ---------------------
	// if (requireFileInput)
	// {
	// 	api.addNonOptionArgument(conf.outDirectory, "out_directory",
	// 			"The directory to write the results to");
	// }
	// else
	// {
	// 	api.addOptionalNonOptionArguments(optionalOutputDir, "[out_directory]",
	// 			"The directory to write the results to.");
	// }

	api.addOptionalNonOptionArguments(optionalOutputDir, "[out_directory]",
			"The directory to write the results to.", "Output");

	api.addIncrementalOptionArgument(conf.verbosity, 'v', "verbose",
			"Verbose output on the console. Add additional 'v' flags to increase verbosity up to -vvv.", "Output");

	api.addOptionArgument(conf.silent, "silent",
			"No output on the console.", "Output");

	api.addOptionArgument(conf.showBiNodes, "show-bipartite-nodes",
			"Include the bipartite nodes in the output.", "Output", true);

	api.parseArgs(flags);

	if (!optionalOutputDir.empty())
		conf.outDirectory = optionalOutputDir[0];

	if (requireFileInput)
		conf.noFileOutput = false;
	else if (conf.outDirectory == "")
		conf.noFileOutput = true;

	if (!conf.noFileOutput && conf.outDirectory == "" && requireFileInput)
	{
		throw InputDomainError("Missing out_directory");
	}

	// Some checks
	if (*--conf.outDirectory.end() != '/')
		conf.outDirectory.append("/");

	if (conf.haveOutput() && !isDirectoryWritable(conf.outDirectory))
		throw FileOpenError(io::Str() << "Can't write to directory '" <<
			conf.outDirectory << "'. Check that the directory exists and that you have write permissions.");

	if (conf.outName.empty()) {
		if (conf.networkFile != "")
			conf.outName = FileURI(conf.networkFile).getName();
		else
			conf.outName = "no-name";
	}

	conf.parsedString = flags;
	conf.parsedOptions = api.getUsedOptionArguments();

	conf.adaptDefaults();

	Log::init(conf.verbosity, conf.silent, conf.verboseNumberPrecision);

	return conf;
}

}
