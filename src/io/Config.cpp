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

Config Config::fromString(std::string flags, bool isCLI)
{
	Config conf;
	conf.isCLI = isCLI;

	ProgramInterface api("Infomap",
		"Implementation of the Infomap clustering algorithm based on the Map Equation (see www.mapequation.org)",
		INFOMAP_VERSION);

	api.setGroups({"Input", "Algorithm", "Accuracy", "Output"});

	std::vector<std::string> optionalOutputDir; // Used if !isCLI
	// --------------------- Input options ---------------------
	if (isCLI)
	{
		api.addNonOptionArgument(conf.networkFile, "network_file",
			"File containing the network data. Assumes a link list format if no Pajek formatted heading.", "Input");

		// api.addOptionalNonOptionArguments(conf.additionalInput, "[additional input]",
		// 		"More network layers for multilayer.", true);
	}
	else
	{
		api.addOptionArgument(conf.networkFile, "input",
			"File containing the network data. Assumes a link list format if no Pajek formatted heading.", ArgType::path, "Input");
	}

	api.addOptionArgument(conf.inputFormat, 'i', "input-format",
			"Override automatically recognized types. Options: pajek, link-list, bipartite, multilayer, states.", ArgType::option, "Input", true);

	api.addOptionArgument(conf.skipAdjustBipartiteFlow, "skip-adjust-bipartite-flow",
			"Skip distributing all flow from the bipartite nodes (first column) to the ordinary nodes (second column).", "Input", true);

	api.addOptionArgument(conf.weightThreshold, "weight-threshold",
			"Limit the number of links to read from the network. Ignore links with less weight than the threshold.", ArgType::number, "Input", true);

	api.addOptionArgument(conf.includeSelfLinks, 'k', "include-self-links",
			"Include links with the same source and target node.", "Input", true);

	api.addOptionArgument(conf.nodeLimit, "node-limit",
			"Limit the number of nodes to read from the network. Ignore links connected to ignored nodes.", ArgType::integer, "Input", true);

	// TODO: Support in v1.x
	// api.addOptionArgument(conf.preClusterMultilayer, "pre-cluster-multilayer",
	// 		"Pre-cluster multilayer networks layer by layer.", "Input", true);

	api.addOptionArgument(conf.clusterDataFile, 'c', "cluster-data",
			"Provide an initial two-level (clu format) or multi-layer (tree format) solution.", ArgType::path, "Input");

	api.addOptionArgument(conf.assignToNeighbouringModule, "assign-to-neighbouring-module",
			"Assign nodes without module assignments (from --cluster-data) to the module assignment of a neighbouring node if possible.", "Input", true);

	api.addOptionArgument(conf.metaDataFile, "meta-data",
			"Provide meta data (clu format) that should be encoded.", ArgType::path, "Input", true);

	api.addOptionArgument(conf.metaDataRate, "meta-data-rate",
			"Metadata encoding rate. Default is to encode each step.", ArgType::number, "Input", true);

	api.addOptionArgument(conf.unweightedMetaData, "meta-data-unweighted",
			"Don't weight meta data by node flow.", "Input", true);

	api.addOptionArgument(conf.noInfomap, "no-infomap",
			"Don't run the optimizer. Useful to calculate codelength of provided cluster data or to print non-modular statistics.", "Input");

	// --------------------- Output options ---------------------

	api.addOptionArgument(conf.outName, "out-name",
			"Name for the output files, e.g. [output_directory]/[out-name].tree", ArgType::string, "Output", true);

	api.addOptionArgument(conf.noFileOutput, '0', "no-file-output",
			"Don't write output to file.", "Output", true);

	api.addOptionArgument(conf.printTree, "tree",
			"Write a tree file with the modular hierarchy. Automatically enabled if no other output is specified.", "Output");

	api.addOptionArgument(conf.printFlowTree, "ftree",
			"Write a ftree file with the modular hierarchy including aggregated links between (nested) modules. (Used by Network Navigator)", "Output");

	api.addOptionArgument(conf.printClu, "clu",
			"Write a clu file with the top cluster ids for each node.", "Output");

	api.addOptionArgument(conf.cluLevel, "clu-level",
			"For clu output, print modules at specified depth from root. Use -1 for bottom level modules.", ArgType::integer, "Output", true);

	//TODO: Include in -o
	// api.addOptionArgument(conf.printFlowNetwork, "print-flow-network",
	// 		"Print the network with calculated flow values.", "Output", true);

	// -o network,states,clu,ftree
	api.addOptionArgument(conf.outputFormats, 'o', "output",
			"Comma-separated output formats without spaces, e.g. -o clu,tree,ftree. Options: clu, tree, ftree, network, states.", ArgType::list, "Output", true);

	// --------------------- Core algorithm options ---------------------
	api.addOptionArgument(conf.twoLevel, '2', "two-level",
			"Optimize a two-level partition of the network.", "Algorithm");

	api.addOptionArgument(conf.flowModel, 'f', "flow-model",
			"Specify flow model. Options: undirected, directed, undirdir, outdirdir, rawdir.", ArgType::option, "Algorithm");

	api.addOptionArgument(conf.directed, 'd', "directed",
			"Assume directed links. Shorthand for '--flow-model directed'.", "Algorithm");

	// api.addOptionArgument(conf.recordedTeleportation, 'e', "recorded-teleportation",
	// 		"If teleportation is used to calculate the flow, also record it when minimizing codelength.", "Algorithm", true);

	api.addOptionArgument(conf.teleportToNodes, "to-nodes",
			"Teleport to nodes instead of to links, assuming uniform node weights if no such input data.", "Algorithm", true);

	api.addOptionArgument(conf.teleportationProbability, 'p', "teleportation-probability",
			"Probability of teleporting to a random node or link.", ArgType::probability, "Algorithm", true);

	// api.addOptionArgument(conf.selfTeleportationProbability, 'y', "self-link-teleportation-probability",
	// 		"Additional probability of teleporting to itself. Effectively increasing the code rate, generating more and smaller modules.", ArgType::number, "Algorithm", true);

	api.addOptionArgument(conf.markovTime, "markov-time",
			"Scales link flow to change the cost of moving between modules. Higher values results in fewer modules.", ArgType::number, "Algorithm", true);

	api.addOptionArgument(conf.preferredNumberOfModules, "preferred-number-of-modules",
			"Penalize solutions the more they differ from this number.", ArgType::integer, "Algorithm", true);

	api.addOptionArgument(conf.multilayerRelaxRate, "multilayer-relax-rate",
			"Probability to relax the constraint to move only in the current layer.", ArgType::probability, "Algorithm", true);

	api.addOptionArgument(conf.multilayerRelaxLimit, "multilayer-relax-limit",
			"Number of neighboring layers in each direction to relax to. If negative, relax to any layer.", ArgType::integer, "Algorithm", true);

	api.addOptionArgument(conf.multilayerRelaxLimitUp, "multilayer-relax-limit-up",
			"Number of neighboring layers with higher id to relax to. If negative, relax to any layer.", ArgType::integer, "Algorithm", true);

	api.addOptionArgument(conf.multilayerRelaxLimitDown, "multilayer-relax-limit-down",
			"Number of neighboring layers with lower id to relax to. If negative, relax to any layer.", ArgType::integer, "Algorithm", true);

	//TODO: Add for v1.x
	// api.addOptionArgument(conf.multilayerRelaxByJensenShannonDivergence, "multilayer-relax-by-jsd",
	// 		"Relax proportional to the out-link similarity measured by the Jensen-Shannon divergence.", ArgType::number, "Algorithm", true);

	// api.addOptionArgument(conf.multilayerJSRelaxRate, "multilayer-js-relax-rate",
	// 		"The probability to relax the constraint to move only in the current layer and instead move to a random layer where the same physical node is present and proportional to the out-link similarity measured by the Jensen-Shannon divergence.", ArgType::getprop, "Algorithm", true);

	// api.addOptionArgument(conf.multilayerJSRelaxLimit, "multilayer-js-relax-limit",
	// 		"The minimum out-link similarity measured by the Jensen-Shannon divergence to relax to other layer. From 0 to 1. No limit if negative.", ArgType::getprop, "Algorithm", true);

	// --------------------- Performance and accuracy options ---------------------
	api.addOptionArgument(conf.seedToRandomNumberGenerator, 's', "seed",
			"A seed (integer) to the random number generator for reproducible results.", ArgType::integer, "Accuracy");

	api.addOptionArgument(conf.numTrials, 'N', "num-trials",
			"Number of outer-most loops to run before picking the best solution.", ArgType::integer, "Accuracy");

	// api.addOptionArgument(conf.randomizeCoreLoopLimit, "random-loop-limit",
	// 		"Randomize the core loop limit from 1 to 'core-loop-limit'", "Accuracy", true);

	api.addOptionArgument(conf.coreLoopLimit, 'M', "core-loop-limit",
			"Limit the number of loops that tries to move each node into the best possible module.", ArgType::integer, "Accuracy", true);

	api.addOptionArgument(conf.levelAggregationLimit, 'L', "core-level-limit",
			"Limit the number of times the core loops are reapplied on existing modular network to search bigger structures.", ArgType::integer, "Accuracy", true);

	api.addOptionArgument(conf.tuneIterationLimit, 'T', "tune-iteration-limit",
			"Limit the number of main iterations in the two-level partition algorithm. 0 means no limit.", ArgType::integer, "Accuracy", true);

	api.addOptionArgument(conf.minimumCodelengthImprovement, "core-loop-codelength-threshold",
			"Minimum codelength threshold for accepting a new solution in core loop.", ArgType::number, "Accuracy", true);

	api.addOptionArgument(conf.minimumRelativeTuneIterationImprovement, "tune-iteration-relative-threshold",
			"Set codelength improvement threshold of each new tune iteration to 'f' times the initial two-level codelength.", ArgType::number, "Accuracy", true);

	// api.addOptionArgument(conf.fastFirstIteration, "fast-first-iteration",
	// 		"Move nodes to strongest connected module in the first iteration instead of minimizing the map equation.", "Accuracy", true);

	// api.addOptionArgument(conf.fastCoarseTunePartition, 'C', "fast-coarse-tune",
	// 		"Try to find the quickest partition of each module when creating sub-modules for the coarse-tune part.", "Accuracy", true);

	// api.addOptionArgument(conf.alternateCoarseTuneLevel, 'A', "alternate-coarse-tune-level",
	// 		"Try to find different levels of sub-modules to move in the coarse-tune part.", "Accuracy", true);

	// api.addOptionArgument(conf.coarseTuneLevel, 'S', "coarse-tune-level",
	// 		"Set the recursion limit when searching for sub-modules. A level of 1 will find sub-sub-modules.", ArgType::integer, "Accuracy", true);

	api.addIncrementalOptionArgument(conf.fastHierarchicalSolution, 'F', "fast-hierarchical-solution",
			"Find top modules fast. Use -FF to keep all fast levels. Use -FFF to skip recursive part.", "Accuracy", true);

	api.addOptionArgument(conf.preferModularSolution, "prefer-modular-solution",
			"Prefer modular solutions even if they are worse than putting all nodes in one module.", "Accuracy", true);

	// api.addIncrementalOptionArgument(conf.lowMemoryPriority, 'l', "low-memory",
	// 		"Prioritize memory efficient algorithms before fast. Use -ll to optimize even more, but this may give approximate results.");

	api.addOptionArgument(conf.innerParallelization, "inner-parallelization",
			"Parallelize the inner-most loop for greater speed. This may give some accuracy tradeoff.", "Accuracy", true);

	// --------------------- Output options ---------------------
	// if (isCLI)
	// {
	// 	api.addNonOptionArgument(conf.outDirectory, "out_directory",
	// 			"The directory to write the results to");
	// }
	// else
	// {
	// 	api.addOptionalNonOptionArguments(optionalOutputDir, "[out_directory]",
	// 			"The directory to write the results to.");
	// }

	api.addOptionalNonOptionArguments(optionalOutputDir, "out_directory",
			"Directory to write the results to.", "Output");

	api.addIncrementalOptionArgument(conf.verbosity, 'v', "verbose",
			"Verbose output on the console. Add additional 'v' flags to increase verbosity up to -vvv.", "Output");

	api.addOptionArgument(conf.silent, "silent",
			"No output on the console.", "Output");

	// api.addOptionArgument(conf.showBiNodes, "show-bipartite-nodes",
	// 		"Include the bipartite nodes in the output.", "Output", true);

	api.parseArgs(flags);

	if (!optionalOutputDir.empty())
		conf.outDirectory = optionalOutputDir[0];

	if (!isCLI && conf.outDirectory == "")
		conf.noFileOutput = true;

	if (!conf.noFileOutput && conf.outDirectory == "" && isCLI)
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


void Config::adaptDefaults()
{
	if (flowModel != FlowModel::undirected && \
		flowModel != FlowModel::undirdir && \
		flowModel != FlowModel::directed && \
		flowModel != FlowModel::outdirdir && \
		flowModel != FlowModel::rawdir)
	{
		throw InputDomainError("Unrecognized flow model");
	}

	if (undirdir) {
		flowModel = FlowModel::undirdir;
	} else if (directed) {
		flowModel = FlowModel::directed;
	} else if (outdirdir) {
		flowModel = FlowModel::outdirdir;
	} else if (rawdir) {
		flowModel = FlowModel::rawdir;
	}

	auto outputs = io::split(outputFormats, ',');
	for (std::string& o : outputs) {
		if (o == "clu") {
			printClu = true;
		} else if (o == "tree") {
			printTree = true;
		} else if (o == "ftree") {
			printFlowTree = true;
		} else if (o == "network") {
			printPajekNetwork = true;
		} else if (o == "states") {
			printStateNetwork = true;
		} else {
			throw InputDomainError(io::Str() << "Unrecognized output format: '" << o << "'.");
		}
	}

	// Of no output format specified, use tree as default (if not used as a library).
	if (isCLI && !haveModularResultOutput()) {
		printTree = true;
	}


	// if (!haveModularResultOutput())
	// 	printTree = true;

	originallyUndirected = isUndirectedFlow();
	// if (isMemoryNetwork())
	// {
		// if (isMultilayerNetwork())
		// {
			// Include self-links in multilayer networks as layer and node numbers are unrelated
			// includeSelfLinks = true;
			// if (!isUndirectedFlow())
			// {
			// 	// teleportToNodes = true;
			// 	recordedTeleportation = false;
			// }
		// }
		// else
		// {
			// teleportToNodes = true;
			// recordedTeleportation = false;
			// if (isUndirectedFlow()) {
			// 	flowModel = FlowModel::directed;
			// }
		// }
		// if (is3gram()) {
		// 	// Teleport to start of physical chains
		// 	teleportToNodes = true;
		// }
	// }
	// if (isBipartite())
	// {
	// 	bipartite = true;
	// }

	// directedEdges = !isUndirected();
}

}
