/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Infomap.h"

namespace infomap {

std::map<unsigned int, unsigned int> InfomapWrapper::getModules(int level, bool states)
{
  std::map<unsigned int, unsigned int> modules;
  if (haveMemory() && !states) {
    for (auto it(iterTreePhysical(level)); !it.isEnd(); ++it) {
      auto& node = *it;
      if (node.isLeaf()) {
        modules[node.physicalId] = it.moduleId();
      }
    }
  } else {
    for (auto it(iterTree(level)); !it.isEnd(); ++it) {
      auto& node = *it;
      if (node.isLeaf()) {
        modules[states ? node.stateId : node.physicalId] = it.moduleId();
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
        auto& node = *it;
        if (node.isLeaf()) {
          modules[node.physicalId].push_back(it.moduleId());
        }
      }
    } else {
      for (auto it(iterTree(level)); !it.isEnd(); ++it) {
        auto& node = *it;
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

