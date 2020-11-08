/*
 * Infomap.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#ifndef _INFOMAPCORE_H_
#define _INFOMAPCORE_H_

#include "../io/Config.h"
#include "InfomapBase.h"
#include "MapEquation.h"
#include "MemMapEquation.h"
#include "MetaMapEquation.h"
#include "BiasedMapEquation.h"
#include "InfomapOptimizer.h"
#include <memory>

namespace infomap {

class InfomapCore : public InfomapBase {
  using OptimizerPtr = std::unique_ptr<InfomapOptimizerBase>;

protected:
public:
  InfomapCore() : InfomapBase() { initOptimizer(); }
  InfomapCore(const std::string flags) : InfomapBase(flags) { initOptimizer(); }
  InfomapCore(const Config& conf) : InfomapBase(conf) { initOptimizer(); }
  ~InfomapCore() override = default;

  // ===================================================
  // IO
  // ===================================================

  std::ostream& toString(std::ostream& out) const final
  {
    return m_optimizer->toString(out);
  }

  // ===================================================
  // Getters
  // ===================================================

  double getCodelength() const final
  {
    return m_optimizer->getCodelength();
  }

  double getIndexCodelength() const final
  {
    return m_optimizer->getIndexCodelength();
  }

  double getModuleCodelength() const final
  {
    return m_hierarchicalCodelength - m_optimizer->getIndexCodelength();
  }

  double getMetaCodelength(bool unweighted = false) const final
  {
    return m_optimizer->getMetaCodelength(unweighted);
  }

protected:
  void initOptimizer(bool forceNoMemory = false) final
  {
    if (haveMetaData()) {
      m_optimizer = OptimizerPtr(new InfomapOptimizer<MetaMapEquation>());
    } else if (haveMemory() && !forceNoMemory) {
      m_optimizer = OptimizerPtr(new InfomapOptimizer<MemMapEquation>());
    } else {
      m_optimizer = OptimizerPtr(new InfomapOptimizer<BiasedMapEquation>());
    }
    m_optimizer->init(this);
  }

  InfomapBase* getNewInfomapInstance() const final
  {
    return new InfomapCore();
  }

  InfomapBase* getNewInfomapInstanceWithoutMemory() const final
  {
    auto im = new InfomapCore();
    im->initOptimizer(true);
    return im;
  }

  unsigned int numActiveModules() const final
  {
    return m_optimizer->numActiveModules();
  }

  // ===================================================
  // Run: Init: *
  // ===================================================

  // Init terms that is constant for the whole network
  void initNetwork() final
  {
    return m_optimizer->initNetwork();
  }

  void initSuperNetwork() final
  {
    return m_optimizer->initSuperNetwork();
  }

  double calcCodelength(const InfoNode& parent) const final
  {
    return m_optimizer->calcCodelength(parent);
  }

  // ===================================================
  // Run: Partition: *
  // ===================================================

  void initPartition() final
  {
    return m_optimizer->initPartition();
  }

  void moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules) final
  {
    return m_optimizer->moveActiveNodesToPredefinedModules(modules);
  }

  unsigned int optimizeActiveNetwork() final
  {
    return m_optimizer->optimizeActiveNetwork();
  }

  unsigned int tryMoveEachNodeIntoBestModule()
  {
    return m_optimizer->tryMoveEachNodeIntoBestModule();
  }

  unsigned int tryMoveEachNodeIntoBestModuleInParallel()
  {
    return m_optimizer->tryMoveEachNodeIntoBestModuleInParallel();
  }

  void consolidateModules(bool replaceExistingModules = true) final
  {
    return m_optimizer->consolidateModules(replaceExistingModules);
  }

  bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false) final
  {
    return m_optimizer->restoreConsolidatedOptimizationPointIfNoImprovement(forceRestore);
  }

  // ===================================================
  // Debug: *
  // ===================================================

  void printDebug() const final
  {
    return m_optimizer->printDebug();
  }

  // ===================================================
  // Protected members
  // ===================================================

  OptimizerPtr m_optimizer;
};


} /* namespace infomap */

#endif /* _INFOMAPCORE_H_ */
