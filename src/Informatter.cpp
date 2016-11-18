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



#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include "utils/Logger.h"
#include "io/Config.h"
#include "infomap/InfomapContext.h"
#include "utils/Stopwatch.h"
#include "io/ProgramInterface.h"
#include "io/convert.h"
#include "utils/FileURI.h"
#include "utils/Date.h"
#include <iomanip>
#include "io/version.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

std::vector<ParsedOption> getConfig(Config& conf, const std::string& args)
{
	ProgramInterface api("Informatter", "Infomap formatter utility", INFOMAP_VERSION);

	api.addProgramDescription(io::Str() <<
		"Convert data to different formats for the Infomap clustering algorithm (see www.mapequation.org)\n" <<
		"\nExamples:\n" <<
		"-------------\n" <<
		"Convert a binary format to human readable tree formats:\n" <<
		"  ./Informatter my_network.bftree output/ --tree --map\n" <<
		"\n" <<
		"Rewrite a network with possible self-links ignored and opposite links aggregated (treated as undirected by default):\n" <<
		"  ./Informatter my_network.net output/ --pajek\n" <<
		"\n" <<
		"Convert a .tree file to a .bftree file with directed links by providing the source network:\n" <<
		"  ./Informatter my_network.net -c my_network.tree -d --bftree\n");


	// --------------------- Input options ---------------------
	api.addNonOptionArgument(conf.networkFile, "input_data",
			"The file containing the network (or binary tree if .btree or .bftree format)");

	api.addOptionalNonOptionArguments(conf.additionalInput, "[additional input]",
			"More network layers for multiplex.", true);

	api.addOptionArgument(conf.inputFormat, 'i', "input-format",
			"Specify input format ('pajek', 'link-list', '3gram' or 'multiplex') to override format possibly implied by file extension.", "s");

	api.addOptionArgument(conf.withMemory, "with-memory",
			"Use second order Markov dynamics and let nodes be part of different modules. Simulate memory from first-order data if not '3gram' input.", true);

	api.addOptionArgument(conf.withMemory, "overlapping",
			"Let nodes be part of different, and thus overlapping, modules. (Same as --with-memory for ordinary networks)");

	api.addOptionArgument(conf.parseWithoutIOStreams, "without-iostream",
			"Parse the input network data without the iostream library. Can be a bit faster, but not as robust.", true);

	api.addOptionArgument(conf.zeroBasedNodeNumbers, 'z', "zero-based-numbering",
			"Assume node numbers start from zero in the input file instead of one.");

	api.addOptionArgument(conf.includeSelfLinks, 'k', "include-self-links",
			"Include links with the same source and target node. (Ignored by default.)");

	api.addOptionArgument(conf.nodeLimit, 'O', "node-limit",
			"Limit the number of nodes to read from the network. Ignore links connected to ignored nodes.", "n");

	api.addOptionArgument(conf.clusterDataFile, 'c', "cluster-data",
			"Provide an initial two-level solution (.clu format).", "p");

	// --------------------- Output options ---------------------
	api.addOptionArgument(conf.noFileOutput, '0', "no-file-output",
			"Don't print any output to file.", true);

	api.addOptionArgument(conf.printMap, "map",
			"Print the top two-level modular network in the .map format.");

	api.addOptionArgument(conf.printClu, "clu",
			"Print the top cluster indices for each node.");

	api.addOptionArgument(conf.printTree, "tree",
			"Print the hierarchy in .tree format. (default true if no other output with cluster data)");

	api.addOptionArgument(conf.printFlowTree, "ftree",
			"Print the hierarchy in .tree format and append the hierarchically aggregated network links.");

	api.addOptionArgument(conf.printBinaryTree, "btree",
			"Print the tree in a streamable binary format.", false);

	api.addOptionArgument(conf.printBinaryFlowTree, "bftree",
			"Print the tree including horizontal flow links in a streamable binary format.");

	api.addOptionArgument(conf.printNodeRanks, "node-ranks",
			"Print the calculated flow for each node to a file.");

	api.addOptionArgument(conf.printFlowNetwork, "flow-network",
			"Print the network with calculated flow values.");

	api.addOptionArgument(conf.printPajekNetwork, "pajek",
			"Print the parsed network in Pajek format.");

	api.addOptionArgument(conf.printExpanded, "expanded",
			"Print the expanded network of memory nodes if possible.");

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

	api.addOptionArgument(conf.multiplexRelaxRate, "multiplex-aggregation-rate",
			"The probability of following a link as if the layers where completely aggregated. Zero means completely disconnected layers.", "f", true);

	api.addOptionArgument(conf.multiplexJSRelaxRate, "multiplex-js-aggregation-rate",
			"The probability of following a link as if the layers where completely aggregated, weighting by the Jensen-Shannon divergence between the out-links. Zero means completely disconnected layers.", "f", true);

	api.addOptionArgument(conf.multiplexJSRelaxLimit, "multiplex-js-relax-limit",
			"The minimum out-link similarity measured by the Jensen-Shannon divergence to relax to other layer. From 0 to 1. No limit if negative.", "f", true);

	api.addOptionArgument(conf.multiplexRelaxLimit, "multiplex-aggregation-limit",
			"The number of neighboring layers in each direction to relax to. If negative, relax to any layer.", "n", true);

	api.addIncrementalOptionArgument(conf.lowMemoryPriority, 'l', "low-memory",
			"Prioritize memory efficient algorithms before fast. Use -ll to optimize even more, but this may give approximate results.");

	// --------------------- Output options ---------------------
	api.addNonOptionArgument(conf.outDirectory, "out_directory",
			"The directory to write the results to");

	api.addIncrementalOptionArgument(conf.verbosity, 'v', "verbose",
			"Verbose output on the console. Add additional 'v' flags to increase verbosity up to -vvv.");

	api.parseArgs(args);

	conf.parsedArgs = args;

	// Some checks
	if (*--conf.outDirectory.end() != '/')
		conf.outDirectory.append("/");

	if (conf.haveOutput() && !isDirectoryWritable(conf.outDirectory))
		throw FileOpenError(io::Str() << "Can't write to directory '" <<
				conf.outDirectory << "'. Check that the directory exists and that you have write permissions.");

	return api.getUsedOptionArguments();
}

void runInformatter(Config const& config)
{
	InfomapContext context(config);
	InfomapBase& infomap = *context.getInfomap();
	infomap.initNetwork();

	if (config.clusterDataFile == "")
		return;

	infomap.calcOneLevelCodelength();
	infomap.consolidateExternalClusterData(true);
}

int run(const std::string& args)
{
	Date startDate;
	Config conf;
	try
	{
		std::vector<ParsedOption> flags = getConfig(conf, args);

		Log::init(conf.verbosity, conf.silent, conf.verboseNumberPrecision);

		Log() << "=======================================================\n";
		Log() << "  Informatter v" << INFOMAP_VERSION << " starts at " << Date() << "\n";
		Log() << "  -> Input: " << conf.networkFile << "\n";
		Log() << "  -> Output path:   " << conf.outDirectory << "\n";
		if (!flags.empty()) {
			for (unsigned int i = 0; i < flags.size(); ++i)
				Log() << (i == 0 ? "  -> Configuration: " : "                    ") << flags[i] << "\n";
		}
		Log() << "=======================================================\n";

		conf.adaptDefaults();

		Log() << std::setprecision(conf.verboseNumberPrecision);

		runInformatter(conf);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	Log() << "===========================================\n";
	Log() << "  Informatter ends at " << Date() << "\n";
	Log() << "  (Elapsed time: " << (Date() - startDate) << ")\n";
	Log() << "===========================================\n";

	return 0;
}

int main(int argc, char* argv[])
{
	std::ostringstream args("");
	for (int i = 1; i < argc; ++i)
		args << argv[i] << (i + 1 == argc? "" : " ");

	return run(args.str());
}

#ifdef NS_INFOMAP
}
#endif
