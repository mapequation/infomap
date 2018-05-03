/*
 * Infomap.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#ifndef INFOMAP_H_
#define INFOMAP_H_

#include "InfomapBase.h"
#include <set>
#include "../utils/VectorMap.h"
#include "../utils/infomath.h"
#include "InfoNode.h"
#include "FlowData.h"
#include <utility>
#include <tuple>
#include "MapEquation.h"
#include "MemMapEquation.h"
#include <memory>

namespace infomap {

class Infomap : public InfomapBase {
	using Base = InfomapBase;
	using FlowDataType = FlowData;
	using DeltaFlowDataType = MemDeltaFlow;
	// using FlowDataType = typename Objective::FlowDataType;
	// using DeltaFlowDataType = typename Objective::DeltaFlowDataType;
	// template<typename T>
	// using ptr = std::shared_ptr<T>;
	using MapEquationPtr = std::unique_ptr<MapEquation>;

protected:
//	using Base::EdgeType;
//	using EdgeType = Base::EdgeType;
	using EdgeType = Edge<InfoNode>;
public:
	// template<typename... Args>
	// Infomap(Args&&... args) : InfomapBase(std::forward<Args>(args)...) {}
	Infomap(bool forceNoMemory = false) : InfomapBase() { initMapEquation(forceNoMemory); }
	Infomap(const Config& conf) : InfomapBase(conf) { initMapEquation(); }
	virtual ~Infomap() {}

	// ===================================================
	// IO
	// ===================================================

	virtual std::ostream& toString(std::ostream& out) const;

	// ===================================================
	// Getters
	// ===================================================

	using Base::root;
	using Base::numLeafNodes;
	using Base::numTopModules;
	using Base::m_numNonTrivialTopModules;
	using Base::numLevels;

	virtual double getCodelength() const;

	virtual double getIndexCodelength() const;

	virtual double getModuleCodelength() const;

	bool haveMemory() const;

protected:
	Infomap& initMapEquation(bool forceNoMemory = false)
	{
		if (haveMemory() && !forceNoMemory) {
			m_objective = MapEquationPtr(new MemMapEquation());
		} else {
			m_objective = MapEquationPtr(new MapEquation());
		}
		return *this;
	}

	// virtual InfomapBase& getInfomap(InfoNode& node);
	virtual InfomapBase* getNewInfomapInstance() const;
	virtual InfomapBase* getNewInfomapInstanceWithoutMemory() const;

	using Base::isTopLevel;
	using Base::isMainInfomap;

	using Base::isFirstLoop;

	virtual unsigned int numActiveModules() const;

	using Base::activeNetwork;

	std::vector<FlowDataType>& getModuleFlowData() { return m_moduleFlowData; }

	// ===================================================
	// Run: Init: *
	// ===================================================

	// Init terms that is constant for the whole network
	virtual void initNetwork();

	virtual void initSuperNetwork();

	virtual double calcCodelength(const InfoNode& parent) const;

	// ===================================================
	// Run: Partition: *
	// ===================================================

	virtual void initPartition();

	virtual void moveActiveNodesToPredifinedModules(std::vector<unsigned int>& modules);

	virtual unsigned int optimizeActiveNetwork();
	
	unsigned int tryMoveEachNodeIntoBestModule();
	
	// unsigned int tryMoveEachNodeIntoBestModuleLocal();

	unsigned int tryMoveEachNodeIntoBestModuleInParallel();

	virtual void consolidateModules(bool replaceExistingModules = true);

	virtual bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false);

	// ===================================================
	// Debug: *
	// ===================================================

	virtual void printDebug();

	// ===================================================
	// Protected members
	// ===================================================

	using Base::m_oneLevelCodelength;
	using Base::m_rand;
	using Base::m_aggregationLevel;
	using Base::m_isCoarseTune;

	MapEquationPtr m_objective;
	MapEquationPtr m_consolidatedObjective;
	std::vector<FlowDataType> m_moduleFlowData;
	std::vector<unsigned int> m_moduleMembers;
	std::vector<unsigned int> m_emptyModules;
};


} /* namespace infomap */

#endif /* INFOMAP_H_ */
