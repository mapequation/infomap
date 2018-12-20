/*
 * Infomap.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#ifndef INFOMAP_H_
#define INFOMAP_H_

#include "core/InfomapCore.h"
#include "io/Config.h"
#include <string>

namespace infomap {

class Infomap : public InfomapCore {
public:

	Infomap(bool forceNoMemory = false) : InfomapCore(forceNoMemory) {}
	Infomap(const Config& conf) : InfomapCore(conf) {}
	Infomap(const std::string& flags) : InfomapCore(flags) {}
	virtual ~Infomap() {}

	// ===================================================
	// Wrapper methods
	// ===================================================

	void readInputData(std::string filename = "", bool accumulate = true) { m_network.readInputData(filename, accumulate); }

	void addNode(unsigned int id) { m_network.addNode(id); }
	void addNode(unsigned int id, std::string name) { m_network.addNode(id, name); }
	void addNode(unsigned int id, double weight) { m_network.addNode(id, weight); }
	void addNode(unsigned int id, std::string name, double weight) { m_network.addNode(id, name, weight); }
	
	void addName(unsigned int id, std::string name) { m_network.addName(id, name); }

	void addPhysicalNode(unsigned int id, std::string name = "") { m_network.addPhysicalNode(id, name); }
	void addStateNode(unsigned int id, unsigned int physId) { m_network.addStateNode(id, physId); }
	
	void addLink(unsigned int sourceId, unsigned int targetId, double weight = 1.0) { m_network.addLink(sourceId, targetId, weight); }
	void addMultilayerLink(unsigned int layer1, unsigned int n1, unsigned int layer2, unsigned int n2, double weight = 1.0) { m_network.addMultilayerLink(layer1, n1, layer2, n2, weight); }
	void addPath(const std::vector<unsigned int>& nodes, unsigned int markovOrder, double weight = 1.0) { m_network.addPath(nodes, markovOrder, weight); }
	
	void setBipartiteStartId(unsigned int startId) { m_network.setBipartiteStartId(startId); }

	std::map<unsigned int, unsigned int> getModules(int level = 1, bool states = false);
	std::map<unsigned int, std::vector<unsigned int>> getMultilevelModules(bool states = false);
};


} /* namespace infomap */

#endif /* INFOMAP_H_ */
