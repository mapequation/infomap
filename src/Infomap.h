/*
 * Infomap.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#ifndef INFOMAP_H_
#define INFOMAP_H_

#ifdef __cplusplus

#include "core/InfomapCore.h"
#include "io/Config.h"
#include <string>

namespace infomap {

struct InfomapWrapper : public InfomapCore {
public:
  InfomapWrapper() : InfomapCore() {}
  InfomapWrapper(const std::string flags) : InfomapCore(flags) {}
  InfomapWrapper(const Config& conf) : InfomapCore(conf) {}
  virtual ~InfomapWrapper() {}

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

  void setBipartiteStartId(unsigned int startId) { m_network.setBipartiteStartId(startId); }

  std::map<unsigned int, unsigned int> getModules(int level = 1, bool states = false);
  std::map<unsigned int, std::vector<unsigned int>> getMultilevelModules(bool states = false);
	using InfomapCore::run;
	using InfomapCore::codelength;
	using InfomapCore::iterTree;
	using InfomapCore::iterLeafNodes;

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
