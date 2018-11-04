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
#include "utils/Log.h"
#include "io/Config.h"
#include "io/convert.h"
#include <string>
#include "Infomap.h"
#include "utils/exceptions.h"
#include <algorithm>
#ifdef _OPENMP
#include <omp.h>
#endif

namespace infomap {


std::map<unsigned int, unsigned int> Infomap::getModules(int level, bool states)
{
	// int maxDepth = maxTreeDepth();
	// if (level >= maxDepth)
	// 	throw InputDomainError(io::Str() << "Maximum module level is " << maxDepth - 1 << ".");
	// if (level < -1)
	// 	throw InputDomainError(io::Str() << "Minimum module level is -1, meaning finest module level starting from bottom of the tree.");
	std::map<unsigned int, unsigned int> modules;
	if (haveMemory() && !states) {
		for (auto it(iterTreePhysical(level)); !it.isEnd(); ++it) {
			InfoNode &node = *it;
			if (node.isLeaf()) {
				modules[node.physicalId] = it.moduleIndex();
			}
		}
	} else {
		for (auto it(iterTree(level)); !it.isEnd(); ++it) {
			InfoNode &node = *it;
			if (node.isLeaf()) {
				auto nodeId = states ? node.stateId : node.physicalId;
				modules[nodeId] = it.moduleIndex();
			}
		}
	}
	return modules;
}

std::map<unsigned int, std::vector<unsigned int>> Infomap::getMultilevelModules(bool states)
{
	unsigned int maxDepth = maxTreeDepth();
	unsigned int numModuleLevels = maxDepth - 1;
	std::map<unsigned int, std::vector<unsigned int>> modules;
	for (unsigned int level = 1; level <= numModuleLevels; ++level) {
		if (haveMemory() && !states) {
			for (auto it(iterTreePhysical(level)); !it.isEnd(); ++it) {
				InfoNode &node = *it;
				if (node.isLeaf()) {
					modules[node.physicalId].push_back(it.moduleIndex());
				}
			}
		} else {
			for (auto it(iterTree(level)); !it.isEnd(); ++it) {
				InfoNode &node = *it;
				if (node.isLeaf()) {
					auto nodeId = states ? node.stateId : node.physicalId;
					modules[nodeId].push_back(it.moduleIndex());
				}
			}
		}
	}
	return modules;
}

int run(const std::string& flags)
{
	try
	{
		Config conf(flags, true);

		Infomap infomap(conf);
		// Infomap infomap(flags, true);

		infomap.run();
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
