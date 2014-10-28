/*
 * NetworkAdapter.h
 *
 *  Created on: 16 okt 2014
 *      Author: Daniel
 */

#ifndef SRC_INFOMAP_NETWORKADAPTER_H_
#define SRC_INFOMAP_NETWORKADAPTER_H_

#include <string>

#include "../io/Config.h"
#include "TreeData.h"

class NetworkAdapter {
public:
	NetworkAdapter(const Config& config, TreeData& treeData) :
		m_config(config),
		m_treeData(treeData),
		m_numNodes(treeData.numLeafNodes()),
	 	m_indexOffset(m_config.zeroBasedNodeNumbers ? 0 : 1)
	{};
	virtual ~NetworkAdapter() {};

	bool readExternalHierarchy(std::string filename);

protected:
	void readClu(std::string filename);
	void readHumanReadableTree(std::string filename);


protected:
	const Config& m_config;
	TreeData& m_treeData;
	unsigned int m_numNodes;
	unsigned int m_indexOffset;
};

#endif /* SRC_INFOMAP_NETWORKADAPTER_H_ */
