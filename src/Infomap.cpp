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
#include "utils/Log.h"
#include "io/Config.h"
#include "utils/Stopwatch.h"
#include "utils/Date.h"
#include "io/version.h"
#include <string>
#include <memory>
#ifdef _OPENMP
#include <omp.h>
#endif

namespace infomap {

int run(const std::string& flags)
{
	try
	{
		Stopwatch timer(true);

		Config conf(flags, true);

		std::unique_ptr<InfomapBase> myInfomap;
		if (conf.isMemoryNetwork())
			myInfomap = std::unique_ptr<InfomapBase>(new MemInfomap(conf));
		else
			myInfomap = std::unique_ptr<InfomapBase>(new Infomap(conf));

		InfomapBase& infomap = *myInfomap;
		// Infomap infomap(flags, true);

		Log::init(infomap.verbosity, infomap.silent, infomap.verboseNumberPrecision);

		Log() << "=======================================================\n";
		Log() << "  Infomap v" << INFOMAP_VERSION << " starts at " << Date() << "\n";
		Log() << "  -> Input network: " << infomap.networkFile << "\n";
		if (infomap.noFileOutput)
			Log() << "  -> No file output!\n";
		else
			Log() << "  -> Output path:   " << infomap.outDirectory << "\n";
		if (!infomap.parsedOptions.empty()) {
			for (unsigned int i = 0; i < infomap.parsedOptions.size(); ++i)
				Log() << (i == 0 ? "  -> Configuration: " : "                    ") << infomap.parsedOptions[i] << "\n";
		}
		Log() << "  -> Use " << (infomap.isUndirected()? "undirected" : "directed") << " flow and " <<
			(infomap.isMemoryNetwork()? "2nd" : "1st") << " order Markov dynamics";
		if (infomap.useTeleportation())
			Log() << " with " << (infomap.recordedTeleportation ? "recorded" : "unrecorded") << " teleportation to " <<
			(infomap.teleportToNodes ? "nodes" : "links");
		Log() << "\n";
		#ifdef _OPENMP
		#pragma omp parallel
			#pragma omp master
			{
				Log() << "  OpenMP " << _OPENMP << " detected with " << omp_get_num_threads() << " threads...\n";
			}
		#endif
		Log() << "=======================================================\n";

		infomap.run();

		Log() << "===================================================\n";
		Log() << "  Infomap ends at " << Date() << "\n";
		// Log() << "  (Elapsed time: " << (Date() - startDate) << ")\n";
		Log() << "  (Elapsed time: " << timer << ")\n";
		Log() << "===================================================\n";
	}
	catch (std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	catch(char const* e)
	{
		std::cerr << "Str error: " << e << std::endl;
	}

	return 0;
}

}

#ifndef AS_LIB
int main(int argc, char* argv[])
{
	std::ostringstream args("");
	for (int i = 1; i < argc; ++i)
		args << argv[i] << (i + 1 == argc? "" : " ");

	return infomap::run(args.str());

	return 0;
}
#endif
