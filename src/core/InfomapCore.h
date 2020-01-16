/*
 * Infomap.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#ifndef INFOMAP_CORE_H_
#define INFOMAP_CORE_H_

#include "InfomapBase.h"
#include "../io/Config.h"
#include "MapEquation.h"
#include "MemMapEquation.h"
#include "MetaMapEquation.h"
#include "BiasedMapEquation.h"
#include <memory>
#include "InfomapOptimizer.h"

namespace infomap {

class InfomapCore : public InfomapBase {
	using OptimizerPtr = std::unique_ptr<InfomapOptimizerBase>;

protected:
//	using Base::EdgeType;
//	using EdgeType = Base::EdgeType;
	// using EdgeType = Edge<InfoNode>;
public:
	// template<typename... Args>
	// InfomapCore(Args&&... args) : InfomapBase(std::forward<Args>(args)...) {}
	InfomapCore() : InfomapBase() { initOptimizer(); }
	// InfomapCore(bool forceNoMemory = false) : InfomapBase() { initOptimizer(forceNoMemory); }
	InfomapCore(const std::string flags) : InfomapBase(flags) { initOptimizer(); }
	InfomapCore(const Config& conf) : InfomapBase(conf) { initOptimizer(); }
	virtual ~InfomapCore() {}

	// ===================================================
	// IO
	// ===================================================

	virtual std::ostream& toString(std::ostream& out) const {
    return m_optimizer->toString(out);
  }

	// ===================================================
	// Getters
	// ===================================================

	virtual double getCodelength() const {
    return m_optimizer->getCodelength();  
  }

	virtual double getIndexCodelength() const {
    return m_optimizer->getIndexCodelength();  
  }

	virtual double getModuleCodelength() const {
    return m_optimizer->getModuleCodelength();  
  }

	virtual double getMetaCodelength(bool unweighted = false) const { 
		return m_optimizer->getMetaCodelength(unweighted);
	}

protected:
	void initOptimizer(bool forceNoMemory = false)
	{
		if (this->haveMetaData()) {
			m_optimizer = OptimizerPtr(new InfomapOptimizer<MetaMapEquation>());
		} else if (haveMemory() && !forceNoMemory) {
			m_optimizer = OptimizerPtr(new InfomapOptimizer<MemMapEquation>());
		} else {
			// m_optimizer = OptimizerPtr(new InfomapOptimizer<MapEquation>());
			m_optimizer = OptimizerPtr(new InfomapOptimizer<BiasedMapEquation>());
		}
    m_optimizer->init(this);
	}

	virtual InfomapBase* getNewInfomapInstance() const {
    return new InfomapCore();
  }
	virtual InfomapBase* getNewInfomapInstanceWithoutMemory() const {
    auto im = new InfomapCore();
		im->initOptimizer(true);
		return im;
  }

	virtual unsigned int numActiveModules() const {
    return m_optimizer->numActiveModules();
  }

	// ===================================================
	// Run: Init: *
	// ===================================================

	// Init terms that is constant for the whole network
	virtual void initNetwork() {
    return m_optimizer->initNetwork();
  }

	virtual void initSuperNetwork() {
    return m_optimizer->initSuperNetwork();
  }

	virtual double calcCodelength(const InfoNode& parent) const {
    return m_optimizer->calcCodelength(parent);
  }

	// ===================================================
	// Run: Partition: *
	// ===================================================

	virtual void initPartition() {
    return m_optimizer->initPartition();
  }

	virtual void moveActiveNodesToPredifinedModules(std::vector<unsigned int>& modules) {
    return m_optimizer->moveActiveNodesToPredifinedModules(modules);
  }

	virtual unsigned int optimizeActiveNetwork() {
    return m_optimizer->optimizeActiveNetwork();
  }
	
	virtual unsigned int tryMoveEachNodeIntoBestModule() {
    return m_optimizer->tryMoveEachNodeIntoBestModule();
  }
	
	// unsigned int tryMoveEachNodeIntoBestModuleLocal() {
  // }

	virtual unsigned int tryMoveEachNodeIntoBestModuleInParallel() {
    return m_optimizer->tryMoveEachNodeIntoBestModuleInParallel();
  }

	virtual void consolidateModules(bool replaceExistingModules = true) {
    return m_optimizer->consolidateModules(replaceExistingModules);
  }

	virtual bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false) {
    return m_optimizer->restoreConsolidatedOptimizationPointIfNoImprovement(forceRestore);
  }

	// ===================================================
	// Debug: *
	// ===================================================

	virtual void printDebug() {
    return m_optimizer->printDebug();
  }

	// ===================================================
	// Protected members
	// ===================================================

  OptimizerPtr m_optimizer;
};


} /* namespace infomap */

#endif /* INFOMAP_CORE_H_ */
