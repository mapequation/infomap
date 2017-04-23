/*
 * InfoNode.h
 *
 *  Created on: 19 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_INFONODE_H_
#define SRC_CLUSTERING_INFONODE_H_

#include "InfoNodeBase.h"
#include "InfomapBase.h"
#include <memory>
#include "InfoNodeTraits.h"
#include "InfomapTypes.h"

namespace infomap {

class InfoNode : public InfoNodeBase
{
protected:
	template<typename Node>
	using DefaultInfomapType = typename default_infomap<false>::template type<Node>;
	template<typename Node>
	using DefaultMemInfomapType = typename default_infomap<true>::template type<Node>;

	std::unique_ptr<InfomapBase<InfoNode>> m_infomap;


public:
	InfoNode() : InfoNodeBase() {}
	InfoNode(const FlowData& flowData) : InfoNodeBase(flowData) {}
	InfoNode(const FlowData& flowData, unsigned int stateId) : InfoNodeBase(flowData, stateId) {}
	InfoNode(const FlowData& flowData, unsigned int stateId, unsigned int physicalId) : InfoNodeBase(flowData, stateId, physicalId) {}
	InfoNode(const InfoNode& other) : InfoNodeBase(other) {}

	// ---------------------------- Infomap ----------------------------

//	template<template<typename, typename> class _Infomap = Infomap, template<typename> class Objective = MapEquation, template<typename, typename> class Optimizer = GreedyOptimizer>
//	InfomapBase<InfoNode>& getInfomap(bool reset = false);

//	template<template<typename> class Infomap = M1Infomap>
//	template<template<typename> class Infomap = default_infomap<Key>::InfomapType>
	// template<template<typename> class Infomap = DefaultInfomapType>
	InfomapBase<InfoNode>& getInfomap(bool reset = false);

	InfomapBase<InfoNode>& getMemInfomap(bool reset = false);

	virtual InfoNodeBase* getInfomapRoot() {
		return m_infomap? &m_infomap->root() : nullptr;
	}

	/**
	 * Dispose the Infomap instance if it exists
	 * @return true if an existing Infomap instance was deleted
	 */
	bool disposeInfomap();

	// ---------------------------- Mutators ----------------------------

};

//template<typename Node>
//template<template<typename, typename> class _Infomap = Infomap, template<typename> class Objective, template<typename, typename> class Optimizer>
//InfomapBase<Node>& InfoNode<Node>::getInfomap(bool reset) {
//	if (!m_infomap || reset)
//		m_infomap = std::unique_ptr<InfomapBase<Node>>(new _Infomap<Node, Optimizer<Node, Objective<Node>>>());
//	return *m_infomap;
//}


// template<template<typename> class _Infomap>
inline
InfomapBase<InfoNode>& InfoNode::getInfomap(bool reset) {
	if (!m_infomap || reset)
		m_infomap = std::unique_ptr<InfomapBase<InfoNode>>(new DefaultInfomapType<InfoNode>());
	return *m_infomap;
}

inline
InfomapBase<InfoNode>& InfoNode::getMemInfomap(bool reset) {
	if (!m_infomap || reset)
		m_infomap = std::unique_ptr<InfomapBase<InfoNode>>(new DefaultMemInfomapType<InfoNode>());
	return *m_infomap;
}

inline
bool InfoNode::disposeInfomap()
{
	if (m_infomap) {
		m_infomap.reset();
		return true;
	}
	return false;
}

}

#endif /* SRC_CLUSTERING_INFONODE_H_ */
