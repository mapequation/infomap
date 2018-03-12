/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

 For more information, see <http://www.mapequation.org>


 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/

#include "Infomap.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
// #include <fenv.h>
#include "utils/Logger.h"
#include "utils/Stopwatch.h"
#include "io/ProgramInterface.h"
#include "io/convert.h"
#include "utils/FileURI.h"
#include "utils/Date.h"
#include "io/version.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

void runInfomap(Config const& config)
{
	InfomapContext context(config);
	context.getInfomap()->run();
}

void runInfomap(Config const& config, Network& input, HierarchicalNetwork& output)
{
	InfomapContext context(config);
	context.getInfomap()->run(input, output);
}

std::vector<ParsedOption> getConfig(Config& conf, const std::string& flags, bool noFileIO = false)
{
	ProgramInterface api("Infomap",
			"Implementation of the Infomap clustering algorithm based on the Map Equation (see www.mapequation.org)",
			INFOMAP_VERSION);

	std::vector<std::string> optionalOutputDir; // Used if noFileIO
	// --------------------- Input options ---------------------
	if (!noFileIO)
	{
		api.addNonOptionArgument(conf.networkFile, "network_file",
				"The file containing the network data. Accepted formats: Pajek (implied by .net) and link list (.txt)");

		api.addOptionalNonOptionArguments(conf.additionalInput, "[additional input]",
				"More network layers for multilayer networks.", true);
	}
	else
		conf.networkFile = "no-name";

	api.addOptionArgument(conf.inputFormat, 'i', "input-format",
			"Specify input format ('pajek', 'link-list', 'states', '3gram', 'multilayer' or 'bipartite') to override format possibly implied by file extension.", "s");

	api.addOptionArgument(conf.withMemory, "with-memory",
			"Use second order Markov dynamics and let nodes be part of different modules. Simulate memory from first-order data if not '3gram' input.", true);

//	api.addOptionArgument(conf.bipartite, "bipartite",
//			"Let the source id of a link belong to a different kind of nodes and ignore that set in the output.");

	api.addOptionArgument(conf.multiplexAddMissingNodes, "multilayer-add-missing-nodes",
			"Adjust multilayer network so that the same set of physical nodes exist in all layers.", true);

	api.addOptionArgument(conf.multiplexAddMissingNodes, "multiplex-add-missing-nodes",
			"[Deprecated, use multilayer-add-missing-nodes] Adjust multilayer network so that the same set of physical nodes exist in all layers.", true);

	api.addOptionArgument(conf.skipAdjustBipartiteFlow, "skip-adjust-bipartite-flow",
			"Skip distributing all flow from the bipartite nodes (first column) to the ordinary nodes (second column).", true);

	api.addOptionArgument(conf.withMemory, "overlapping",
			"Let nodes be part of different and overlapping modules. Applies to ordinary networks by first representing the memoryless dynamics with memory nodes.");

	api.addOptionArgument(conf.hardPartitions, "hard-partitions",
			"Don't allow overlapping modules in memory networks by keeping the memory nodes constrained into their physical nodes.", true);

	api.addOptionArgument(conf.nonBacktracking, "non-backtracking",
			"Use non-backtracking dynamics and let nodes be part of different and overlapping modules. Applies to ordinary networks by first representing the non-backtracking dynamics with memory nodes.", true);

	api.addOptionArgument(conf.parseWithoutIOStreams, "without-iostream",
			"Parse the input network data without the iostream library. Can be a bit faster, but not as robust.", true);

	api.addOptionArgument(conf.zeroBasedNodeNumbers, 'z', "zero-based-numbering",
			"Assume node numbers start from zero in the input file instead of one.");

	api.addOptionArgument(conf.includeSelfLinks, 'k', "include-self-links",
			"Include links with the same source and target node. (Ignored by default.)");

	api.addOptionArgument(conf.completeDanglingMemoryNodes, "complete-dangling-memory-nodes",
			"Add first order links to complete dangling memory nodes.");

	api.addOptionArgument(conf.nodeLimit, 'O', "node-limit",
			"Limit the number of nodes to read from the network. Ignore links connected to ignored nodes.", "n", true);
	
	api.addOptionArgument(conf.weightThreshold, "weight-threshold",
			"Limit the number of links to read from the network. Ignore links with less weight than the threshold.", "n", true);

	api.addOptionArgument(conf.preClusterMultiplex, "pre-cluster-multiplex",
			"Pre-cluster multiplex networks layer by layer.", true);

	api.addOptionArgument(conf.clusterDataFile, 'c', "cluster-data",
			"Provide an initial two-level (.clu format) or multi-layer (.tree format) solution.", "p", true);

	api.addOptionArgument(conf.noInfomap, "no-infomap",
			"Don't run Infomap. Useful if initial cluster data should be preserved or non-modular data printed.", true);

	// --------------------- Output options ---------------------

	api.addOptionArgument(conf.outName, "out-name",
			"Use this name for the output files, like [output_directory]/[out-name].tree", "s", true);

	api.addOptionArgument(conf.noFileOutput, '0', "no-file-output",
			"Don't print any output to file.", true);

	api.addOptionArgument(conf.printMap, "map",
			"Print the top two-level modular network in the .map format.");

	api.addOptionArgument(conf.printClu, "clu",
			"Print the top cluster indices for each node.");

	api.addOptionArgument(conf.printTree, "tree",
			"Print the hierarchy in .tree format. (default true if no other output with cluster data)");

	api.addOptionArgument(conf.printFlowTree, "ftree",
			"Print the hierarchy in .tree format and append the hierarchically aggregated network links.", true);

	api.addOptionArgument(conf.printBinaryTree, "btree",
			"Print the tree in a streamable binary format.", true);

	api.addOptionArgument(conf.printBinaryFlowTree, "bftree",
			"Print the tree including horizontal flow links in a streamable binary format.");

	api.addOptionArgument(conf.printNodeRanks, "node-ranks",
			"Print the calculated flow for each node to a file.", true);

	api.addOptionArgument(conf.printFlowNetwork, "flow-network",
			"Print the network with calculated flow values.", true);

	api.addOptionArgument(conf.printPajekNetwork, "pajek",
			"Print the parsed network in Pajek format.", true);

	api.addOptionArgument(conf.printStateNetwork, "print-state-network",
			"Print the internal state network.", true);

	api.addOptionArgument(conf.printExpanded, "expanded",
			"Print the expanded network of memory nodes if possible.", true);

	api.addOptionArgument(conf.printAllTrials, "print-all-trials",
			"Print result to file for all trials (if more than one), with the trial number in each file.", true);

	// --------------------- Core algorithm options ---------------------
	api.addOptionArgument(conf.twoLevel, '2', "two-level",
			"Optimize a two-level partition of the network.");

	bool dummyUndirected;
	api.addOptionArgument(dummyUndirected, 'u', "undirected",
			"Assume undirected links. (default)");

	api.addOptionArgument(conf.directed, 'd', "directed",
			"Assume directed links.");

	api.addOptionArgument(conf.undirdir, 't', "undirdir",
			"Two-mode dynamics: Assume undirected links for calculating flow, but directed when minimizing codelength.");

	api.addOptionArgument(conf.outdirdir, "outdirdir",
			"Two-mode dynamics: Count only ingoing links when calculating the flow, but all when minimizing codelength.", true);

	api.addOptionArgument(conf.rawdir, 'w', "rawdir",
			"Two-mode dynamics: Assume directed links and let the raw link weights be the flow.", true);

	api.addOptionArgument(conf.recordedTeleportation, 'e', "recorded-teleportation",
			"If teleportation is used to calculate the flow, also record it when minimizing codelength.", true);

	api.addOptionArgument(conf.teleportToNodes, 'o', "to-nodes",
			"Teleport to nodes instead of to links, assuming uniform node weights if no such input data.", true);

	api.addOptionArgument(conf.teleportationProbability, 'p', "teleportation-probability",
			"The probability of teleporting to a random node or link.", "f", true);

	api.addOptionArgument(conf.selfTeleportationProbability, 'y', "self-link-teleportation-probability",
			"Additional probability of teleporting to itself. Effectively increasing the code rate, generating more and smaller modules.", "f", true);

	api.addOptionArgument(conf.markovTime, "markov-time",
			"Scale link flow with this value to change the cost of moving between modules. Higher for less modules.", "f", true);

	api.addOptionArgument(conf.variableMarkovTime, "variable-markov-time",
			"Scale link flow per node inversely proportional to the node exit entropy to easier split connected hubs and keep long chains together.", true);

	api.addOptionArgument(conf.preferredNumberOfModules, "preferred-number-of-modules",
			"Stop merge or split modules if preferred number of modules is reached.", "n", true);

	api.addOptionArgument(conf.multiplexRelaxRate, "multilayer-relax-rate",
			"The probability to relax the constraint to move only in the current layer and instead move to a random layer where the same physical node is present. If negative, the inter-links have to be provided.", "f", true);

	api.addOptionArgument(conf.multiplexJSRelaxRate, "multilayer-js-relax-rate",
			"The probability to relax the constraint to move only in the current layer and instead move to a random layer where the same physical node is present and proportional to the out-link similarity measured by the Jensen-Shannon divergence. If negative, the inter-links have to be provided.", "f", true);

	api.addOptionArgument(conf.multiplexJSRelaxLimit, "multilayer-js-relax-limit",
			"The minimum out-link similarity measured by the Jensen-Shannon divergence to relax to other layer. From 0 to 1. No limit if negative.", "f", true);

	api.addOptionArgument(conf.multiplexRelaxLimit, "multilayer-relax-limit",
			"The number of neighboring layers in each direction to relax to. If negative, relax to any layer.", "n", true);

	api.addOptionArgument(conf.multiplexRelaxRate, "multiplex-relax-rate",
			"[Deprecated, use multilayer-relax-rate] The probability to relax the constraint to move only in the current layer and instead move to a random layer where the same physical node is present. If negative, the inter-links have to be provided.", "f", true);

	api.addOptionArgument(conf.multiplexJSRelaxRate, "multiplex-js-relax-rate",
			"[Deprecated, use multilayer-js-relax-rate] The probability to relax the constraint to move only in the current layer and instead move to a random layer where the same physical node is present and proportional to the out-link similarity measured by the Jensen-Shannon divergence. If negative, the inter-links have to be provided.", "f", true);

	api.addOptionArgument(conf.multiplexJSRelaxLimit, "multiplex-js-relax-limit",
			"[Deprecated, use multilayer-js-relax-limit] The minimum out-link similarity measured by the Jensen-Shannon divergence to relax to other layer. From 0 to 1. No limit if negative.", "f", true);

	api.addOptionArgument(conf.multiplexRelaxLimit, "multiplex-relax-limit",
			"[Deprecated, use multilayer-relax-limit] The number of neighboring layers in each direction to relax to. If negative, relax to any layer.", "n", true);

	api.addOptionArgument(conf.seedToRandomNumberGenerator, 's', "seed",
			"A seed (integer) to the random number generator.", "n");

	// --------------------- Performance and accuracy options ---------------------
	api.addOptionArgument(conf.numTrials, 'N', "num-trials",
			"The number of outer-most loops to run before picking the best solution.", "n");

	api.addOptionArgument(conf.minimumCodelengthImprovement, 'm', "min-improvement",
			"Minimum codelength threshold for accepting a new solution.", "f", true);

	api.addOptionArgument(conf.randomizeCoreLoopLimit, 'a', "random-loop-limit",
			"Randomize the core loop limit from 1 to 'core-loop-limit'", true);

	api.addOptionArgument(conf.coreLoopLimit, 'M', "core-loop-limit",
			"Limit the number of loops that tries to move each node into the best possible module", "n", true);

	api.addOptionArgument(conf.levelAggregationLimit, 'L', "core-level-limit",
			"Limit the number of times the core loops are reapplied on existing modular network to search bigger structures.", "n", true);

	api.addOptionArgument(conf.tuneIterationLimit, 'T', "tune-iteration-limit",
			"Limit the number of main iterations in the two-level partition algorithm. 0 means no limit.", "n", true);

	api.addOptionArgument(conf.minimumRelativeTuneIterationImprovement, 'U', "tune-iteration-threshold",
			"Set a codelength improvement threshold of each new tune iteration to 'f' times the initial two-level codelength.", "f", true);

	api.addOptionArgument(conf.fastFirstIteration, "fast-first-iteration",
			"Move nodes to strongest connected module in the first iteration instead of minimizing the map equation.", true);

	api.addOptionArgument(conf.fastCoarseTunePartition, 'C', "fast-coarse-tune",
			"Try to find the quickest partition of each module when creating sub-modules for the coarse-tune part.", true);

	api.addOptionArgument(conf.alternateCoarseTuneLevel, 'A', "alternate-coarse-tune-level",
			"Try to find different levels of sub-modules to move in the coarse-tune part.", true);

	api.addOptionArgument(conf.coarseTuneLevel, 'S', "coarse-tune-level",
			"Set the recursion limit when searching for sub-modules. A level of 1 will find sub-sub-modules.", "n", true);

	api.addIncrementalOptionArgument(conf.fastHierarchicalSolution, 'F', "fast-hierarchical-solution",
			"Find top modules fast. Use -FF to keep all fast levels. Use -FFF to skip recursive part.");

	api.addIncrementalOptionArgument(conf.lowMemoryPriority, 'l', "low-memory",
			"Prioritize memory efficient algorithms before fast. Use -ll to optimize even more, but this may give approximate results.", true);

	api.addOptionArgument(conf.innerParallelization, "inner-parallelization",
			"Parallelize the innermost loop for greater speed. Note that this may give some accuracy tradeoff.");

	api.addOptionArgument(conf.resetConfigBeforeRecursion, "reset-options-before-recursion",
			"Reset options tuning the speed and accuracy before the recursive part.", true);

	api.addOptionArgument(conf.showBiNodes, "show-bipartite-nodes",
			"[Deprecated, see --hide-bipartite-nodes] Include the bipartite nodes in the output (now default).", true);

	api.addOptionArgument(conf.hideBipartiteNodes, "hide-bipartite-nodes",
			"Hide the bipartite nodes in the output.", true);

	// --------------------- Output options ---------------------
	if (!noFileIO)
	{
		api.addNonOptionArgument(conf.outDirectory, "out_directory",
				"The directory to write the results to");
	}
	else
	{
		api.addOptionalNonOptionArguments(optionalOutputDir, "[out_directory]",
				"The directory to write the results to.");
	}

	api.addIncrementalOptionArgument(conf.verbosity, 'v', "verbose",
			"Verbose output on the console. Add additional 'v' flags to increase verbosity up to -vvv.");

	api.addOptionArgument(conf.silent, "silent",
			"No output on the console.");

	api.parseArgs(flags);

	conf.parsedArgs = flags;

	if (noFileIO)
	{
		if (!optionalOutputDir.empty())
			conf.outDirectory = optionalOutputDir[0];
		else
			conf.noFileOutput = true;
	}

	// Some checks
	if (*--conf.outDirectory.end() != '/')
		conf.outDirectory.append("/");

	if (conf.haveOutput() && !isDirectoryWritable(conf.outDirectory))
		throw FileOpenError(io::Str() << "Can't write to directory '" <<
				conf.outDirectory << "'. Check that the directory exists and that you have write permissions.");

	if (conf.outName.empty())
		conf.outName = FileURI(conf.networkFile).getName();

	return api.getUsedOptionArguments();
}

void initBenchmark(const Config& conf, const std::string& flags)
{
	std::string networkName = FileURI(conf.networkFile).getName();
	std::string logFilename = io::Str() << conf.outDirectory << networkName << ".tsv";
	Logger::setBenchmarkFilename(logFilename);
	std::ostringstream logInfo;
	logInfo << "#benchmark for '" << flags << "'";
	Logger::benchmark(logInfo.str(), 0, 0, 0, 0, true);
	Logger::benchmark("elapsedSeconds\ttag\tcodelength\tnumTopModules\tnumNonTrivialTopModules\ttreeDepth",
			0, 0, 0, 0, true);
	// (todo: fix problem with initializing same static file from different functions to simplify above)
	Log() << "(Writing benchmark log to '" << logFilename << "'...)\n";
}

Config init(const std::string& flags)
{
	Config conf;
	try
	{
		std::vector<ParsedOption> parsedFlags = getConfig(conf, flags, true);

		Log::init(conf.verbosity, conf.silent, conf.verboseNumberPrecision);
		conf.adaptDefaults();

		Log() << "=======================================================\n";
		Log() << "  Infomap v" << INFOMAP_VERSION << " starts at " << Date() << "\n";
		if (!parsedFlags.empty()) {
			for (unsigned int i = 0; i < parsedFlags.size(); ++i)
				Log() << (i == 0 ? "  -> Configuration: " : "                    ") << parsedFlags[i] << "\n";
		}
		Log() << "  -> Use " << (conf.isUndirected()? "undirected" : "directed") << " flow and " <<
			(conf.isMemoryNetwork()? "2nd" : "1st") << " order Markov dynamics";
		if (conf.useTeleportation())
			Log() << " with " << (conf.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " <<
			(conf.teleportToNodes ? "nodes" : "links");
		Log() << "\n";
		Log() << "=======================================================\n";
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return conf;
}

int run(Network& input, HierarchicalNetwork& output)
{
	try
	{
		runInfomap(input.config(), input, output);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}

int run(const std::string& flags)
{
	Date startDate;
	Config conf;
	Stopwatch timer(true);

	try
	{
		std::vector<ParsedOption> parsedFlags = getConfig(conf, flags);

		Log::init(conf.verbosity, conf.silent, conf.verboseNumberPrecision);
		conf.adaptDefaults();

		Log() << "=======================================================\n";
		Log() << "  Infomap v" << INFOMAP_VERSION << " starts at " << Date() << "\n";
		Log() << "  -> Input network: " << conf.networkFile << "\n";
		Log() << "  -> Output path:   " << conf.outDirectory << "\n";
		if (!parsedFlags.empty()) {
			for (unsigned int i = 0; i < parsedFlags.size(); ++i)
				Log() << (i == 0 ? "  -> Configuration: " : "                    ") << parsedFlags[i] << "\n";
		}
		Log() << "  -> Use " << (conf.isUndirected()? "undirected" : "directed") << " flow and " <<
			(conf.isMemoryNetwork()? "2nd" : "1st") << " order Markov dynamics";
		if (conf.useTeleportation())
			Log() << " with " << (conf.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " <<
			(conf.teleportToNodes ? "nodes" : "links");
		Log() << "\n";
		Log() << "=======================================================\n";

		if (conf.benchmark)
			initBenchmark(conf, flags);

		runInfomap(conf);

	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	ASSERT(NodeBase::nodeCount() == 0); //TODO: Not working with OpenMP
//	if (NodeBase::nodeCount() != 0)
//		Log() << "Warning: " << NodeBase::nodeCount() << " nodes not deleted!\n";


	Log() << "===================================================\n";
	Log() << "  Infomap ends at " << Date() << "\n";
	Log() << "  Elapsed time: " << (Date() - startDate) << " (" << timer.getElapsedTimeInSec() << "s)\n";
	Log() << "===================================================\n";

	return 0;
}

#ifndef AS_LIB
int main(int argc, char* argv[])
{
	std::ostringstream args("");
	for (int i = 1; i < argc; ++i)
		args << argv[i] << (i + 1 == argc? "" : " ");

	int ret = run(args.str());

//   if(fetestexcept(FE_DIVBYZERO))
// 		std::cout << "Warning: division by zero reported" << std::endl;

//   // if(fetestexcept(FE_INEXACT))
// 	// 	std::cout << "Warning: inexact result reported" << std::endl;
// 	// Raised on trivial double arithmetics

//   if(fetestexcept(FE_INVALID))
// 		std::cout << "Warning: invalid result reported" << std::endl;

//   if(fetestexcept(FE_OVERFLOW))
// 		std::cout << "Warning: overflow result reported" << std::endl;

// 	if(fetestexcept(FE_UNDERFLOW))
// 		std::cout << "Warning: underflow result reported" << std::endl;

	// feclearexcept(FE_ALL_EXCEPT);
	return ret;
}
#endif

#ifdef NS_INFOMAP
}
#endif
