/*******************************************************************************
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
 ******************************************************************************/

#ifndef INFOMAP_H_
#define INFOMAP_H_

#ifdef __cplusplus

#include "core/InfomapCore.h"
#include "io/Config.h"

#include <string>
#include <utility>
#include <map>

namespace infomap {

struct InfomapWrapper : public InfomapCore {
public:
  InfomapWrapper() : InfomapCore() { }
  InfomapWrapper(const std::string flags) : InfomapCore(flags) { }
  InfomapWrapper(const Config& conf) : InfomapCore(conf) { }
  virtual ~InfomapWrapper() { }

  // ===================================================
  // Wrapper methods
  // ===================================================

  void readInputData(std::string filename = "", bool accumulate = true) { m_network.readInputData(filename, accumulate); }

  void addNode(unsigned int id) { m_network.addNode(id); }
  void addNode(unsigned int id, std::string name) { m_network.addNode(id, name); }
  void addNode(unsigned int id, double weight) { m_network.addNode(id, weight); }
  void addNode(unsigned int id, std::string name, double weight) { m_network.addNode(id, name, weight); }

  void addName(unsigned int id, std::string name) { m_network.addName(id, name); }
  std::string getName(unsigned int id) const;
  const std::map<unsigned int, std::string>& getNames() const { return m_network.names(); }

  void addPhysicalNode(unsigned int id, std::string name = "") { m_network.addPhysicalNode(id, name); }
  void addStateNode(unsigned int id, unsigned int physId) { m_network.addStateNode(id, physId); }

  void addLink(unsigned int sourceId, unsigned int targetId, double weight = 1.0) { m_network.addLink(sourceId, targetId, weight); }
  void addLink(unsigned int sourceId, unsigned int targetId, unsigned long weight) { m_network.addLink(sourceId, targetId, weight); }
  void addMultilayerLink(unsigned int layer1, unsigned int n1, unsigned int layer2, unsigned int n2, double weight = 1.0) { m_network.addMultilayerLink(layer1, n1, layer2, n2, weight); }
  void addMultilayerIntraLink(unsigned int layer, unsigned int n1, unsigned int n2, double weight) { m_network.addMultilayerIntraLink(layer, n1, n2, weight); }
  void addMultilayerInterLink(unsigned int layer1, unsigned int n, unsigned int layer2, double interWeight) { m_network.addMultilayerInterLink(layer1, n, layer2, interWeight); }

  void setBipartiteStartId(unsigned int startId) { m_network.setBipartiteStartId(startId); }

  std::map<std::pair<unsigned int, unsigned int>, double> getLinks(bool flow);

  std::map<unsigned int, unsigned int> getModules(int level = 1, bool states = false);
  std::map<unsigned int, std::vector<unsigned int>> getMultilevelModules(bool states = false);
  using InfomapCore::codelength;
  using InfomapCore::iterLeafNodes;
  using InfomapCore::iterTree;
  using InfomapCore::run;
};

extern "C" {
#else
#include <stdint.h>
#include <stdbool.h>
struct Infomap;
struct InfomapLeafIterator;
#endif // __cplusplus

#ifndef SWIG

struct InfomapWrapper* NewInfomap(const char* flags);

void DestroyInfomap(struct InfomapWrapper* im);

void InfomapAddLink(struct InfomapWrapper* im, unsigned int sourceId, unsigned int targetId, double weight);

void InfomapRun(struct InfomapWrapper* im);

double Codelength(struct InfomapWrapper* im);

unsigned int NumModules(struct InfomapWrapper* im);

struct InfomapLeafIterator* NewIter(struct InfomapWrapper* im);

void DestroyIter(struct InfomapLeafIterator* it);

bool IsEnd(struct InfomapLeafIterator* it);

void Next(struct InfomapLeafIterator* it);

unsigned int Depth(struct InfomapLeafIterator* it);

unsigned int NodeId(struct InfomapLeafIterator* it);

unsigned int ModuleIndex(struct InfomapLeafIterator* it);

double Flow(struct InfomapLeafIterator* it);

#endif // SWIG
#ifdef __cplusplus
} // extern "C"
} /* namespace infomap */
#endif

#endif /* INFOMAP_H_ */
