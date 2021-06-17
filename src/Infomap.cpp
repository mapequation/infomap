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


std::map<unsigned int, unsigned int> InfomapWrapper::getModules(int level, bool states)
{
  // int maxDepth = maxTreeDepth();
  // if (level >= maxDepth)
  // 	throw InputDomainError(io::Str() << "Maximum module level is " << maxDepth - 1 << ".");
  // if (level < -1)
  // 	throw InputDomainError(io::Str() << "Minimum module level is -1, meaning finest module level starting from bottom of the tree.");
  std::map<unsigned int, unsigned int> modules;
  if (haveMemory() && !states) {
    for (auto it(iterTreePhysical(level)); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        modules[node.physicalId] = it.moduleId();
      }
    }
  } else {
    for (auto it(iterTree(level)); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf()) {
        auto nodeId = states ? node.stateId : node.physicalId;
        modules[nodeId] = it.moduleId();
      }
    }
  }
  return modules;
}

std::map<unsigned int, std::vector<unsigned int>> InfomapWrapper::getMultilevelModules(bool states)
{
  unsigned int maxDepth = maxTreeDepth();
  unsigned int numModuleLevels = maxDepth - 1;
  std::map<unsigned int, std::vector<unsigned int>> modules;
  for (unsigned int level = 1; level <= numModuleLevels; ++level) {
    if (haveMemory() && !states) {
      for (auto it(iterTreePhysical(level)); !it.isEnd(); ++it) {
        InfoNode& node = *it;
        if (node.isLeaf()) {
          modules[node.physicalId].push_back(it.moduleId());
        }
      }
    } else {
      for (auto it(iterTree(level)); !it.isEnd(); ++it) {
        InfoNode& node = *it;
        if (node.isLeaf()) {
          auto nodeId = states ? node.stateId : node.physicalId;
          modules[nodeId].push_back(it.moduleId());
        }
      }
    }
  }
  return modules;
}

std::string InfomapWrapper::getName(unsigned int id) const
{
  auto& names = m_network.names();
  auto it = names.find(id);
  return it != names.end() ? it->second : "";
}

std::map<std::pair<unsigned int, unsigned int>, double> InfomapWrapper::getLinks(bool flow)
{
  std::map<std::pair<unsigned int, unsigned int>, double> links;

  for (const auto& node : m_network.nodeLinkMap()) {
    const auto sourceId = node.first.id;

    for (const auto& link : node.second) {
      const auto targetId = link.first.id;
      links[{ sourceId, targetId }] = flow ? link.second.flow : link.second.weight;
    }
  }

  return links;
}

int run(const std::string& flags)
{
  try {
    Config conf(flags, true);

    InfomapWrapper infomap(conf);
    // InfomapWrapper infomap(flags, true);

    infomap.run();
  } catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  } catch (char const* e) {
    std::cerr << "Str error: " << e << std::endl;
  }

  return 0;
}

////////////////////////////////
// Implementation of c bindings
////////////////////////////////

#ifndef SWIG

InfomapWrapper* NewInfomap(const char* flags)
{
  return new InfomapWrapper(flags);
}

void DestroyInfomap(InfomapWrapper* im) { im->~InfomapWrapper(); }

void InfomapAddLink(InfomapWrapper* im, unsigned int sourceId, unsigned int targetId, double weight)
{
  im->addLink(sourceId, targetId, weight);
}

void InfomapRun(struct InfomapWrapper* im) { im->run(); }

double Codelength(struct InfomapWrapper* im) { return im->getHierarchicalCodelength(); }

unsigned int NumModules(struct InfomapWrapper* im) { return im->numTopModules(); }

struct InfomapLeafIterator* NewIter(struct InfomapWrapper* im) { return new InfomapLeafIterator(&(im->root())); }

void DestroyIter(struct InfomapLeafIterator* it) { it->~InfomapLeafIterator(); }

bool IsEnd(struct InfomapLeafIterator* it) { return it->isEnd(); }

void Next(struct InfomapLeafIterator* it) { it->stepForward(); }

unsigned int Depth(struct InfomapLeafIterator* it) { return it->depth(); }

unsigned int NodeId(struct InfomapLeafIterator* it) { return it->current()->id(); }

unsigned int ModuleIndex(struct InfomapLeafIterator* it) { return it->moduleIndex(); }

double Flow(struct InfomapLeafIterator* it) { return it->current()->data.flow; }
#endif

} // namespace infomap

#ifndef AS_LIB
int main(int argc, char* argv[])
{
  std::ostringstream args("");
  for (int i = 1; i < argc; ++i)
    args << argv[i] << (i + 1 == argc ? "" : " ");

  return infomap::run(args.str());
}
#endif
