/*
 * MemoryNetworkAdapter.h
 *
 *  Created on: 26 jun 2015
 *      Author: Daniel
 */

#ifndef SRC_INFOMAP_MEMORYNETWORKADAPTER_H_
#define SRC_INFOMAP_MEMORYNETWORKADAPTER_H_

#include "NetworkAdapter.h"
#include "Node.h"
#include <map>

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class MemoryNetworkAdapter: public NetworkAdapter {
public:
	MemoryNetworkAdapter(const Config& config, TreeData& treeData) :
		NetworkAdapter(config, treeData)
	{};
	virtual ~MemoryNetworkAdapter() {};

	virtual bool readExternalHierarchy(std::string filename);

protected:
	virtual void readClu(std::string filename);
	virtual void readHumanReadableTree(std::string filename);

	void generateMemoryNodeMap();

	std::map<StateNode, unsigned int> m_memNodeToIndex;
};


#ifdef NS_INFOMAP
} /* namespace infomap */
#endif

#endif /* SRC_INFOMAP_MEMORYNETWORKADAPTER_H_ */
