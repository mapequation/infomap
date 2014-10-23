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
		m_config(config), m_treeData(treeData), m_numNodes(treeData.numLeafNodes()) {};
	virtual ~NetworkAdapter() {};

	void addExternalHierarchy(std::string filename);

protected:
	void readHumanReadableTree(std::string filename);


protected:
	const Config& m_config;
	TreeData& m_treeData;
	unsigned int m_numNodes;
};

#endif /* SRC_INFOMAP_NETWORKADAPTER_H_ */
