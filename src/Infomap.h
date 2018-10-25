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

	void addNode(unsigned int id) { m_network.addNode(id); }
	void addNode(unsigned int id, std::string name) { m_network.addNode(id, name); }
	void addNode(unsigned int id, double weight) { m_network.addNode(id, weight); }
	void addNode(unsigned int id, std::string name, double weight) { m_network.addNode(id, name, weight); }
	
	void addName(unsigned int id, std::string name) { m_network.addName(id, name); }

	void addStateNode(unsigned int id, unsigned int physId) { m_network.addStateNode(id, physId); }
	
	void addLink(unsigned int sourceId, unsigned int targetId, double weight = 1.0) { m_network.addLink(sourceId, targetId, weight); }

	std::map<unsigned int, unsigned int> getModules(unsigned int level = 1, bool states = false);
	std::map<unsigned int, std::vector<unsigned int>> getMultilevelModules(bool states = false);
};


} /* namespace infomap */

#endif /* INFOMAP_H_ */
