/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_CORE_H_
#define INFOMAP_CORE_H_

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
public:
  InfomapCore() : InfomapBase() { initOptimizer(); }
  InfomapCore(const std::string& flags, bool isCli = false) : InfomapBase(flags, isCli) { initOptimizer(); }
  InfomapCore(const Config& conf) : InfomapBase(conf) { initOptimizer(); }
  virtual ~InfomapCore() = default;

  // ===================================================
  // IO
  // ===================================================

  virtual std::ostream& toString(std::ostream& out) const
  {
    return m_optimizer->toString(out);
  }

  // ===================================================
  // Getters
  // ===================================================

  virtual double getCodelength() const
  {
    return m_optimizer->getCodelength();
  }

  virtual double getIndexCodelength() const
  {
    return m_optimizer->getIndexCodelength();
  }

  virtual double getModuleCodelength() const
  {
    return m_hierarchicalCodelength - m_optimizer->getIndexCodelength();
  }

  virtual double getMetaCodelength(bool unweighted = false) const
  {
    return m_optimizer->getMetaCodelength(unweighted);
  }

protected:
  void initOptimizer(bool forceNoMemory = false)
  {
    if (haveMetaData()) {
      m_optimizer = std::make_unique<InfomapOptimizer<MetaMapEquation>>();
    } else if (haveMemory() && !forceNoMemory) {
      m_optimizer = std::make_unique<InfomapOptimizer<MemMapEquation>>();
    } else {
      m_optimizer = std::make_unique<InfomapOptimizer<BiasedMapEquation>>();
    }
    m_optimizer->init(this);
  }

  virtual InfomapBase* getNewInfomapInstance() const
  {
    return new InfomapCore(getConfig());
  }
  virtual InfomapBase* getNewInfomapInstanceWithoutMemory() const
  {
    auto im = new InfomapCore();
    im->initOptimizer(true);
    return im;
  }

  virtual unsigned int numActiveModules() const
  {
    return m_optimizer->numActiveModules();
  }

  // ===================================================
  // Run: Init: *
  // ===================================================

  // Init terms that is constant for the whole network
  virtual void initTree()
  {
    return m_optimizer->initTree();
  }
  virtual void initNetwork()
  {
    return m_optimizer->initNetwork();
  }

  virtual void initSuperNetwork()
  {
    return m_optimizer->initSuperNetwork();
  }

  virtual double calcCodelength(const InfoNode& parent) const
  {
    return m_optimizer->calcCodelength(parent);
  }

  // ===================================================
  // Run: Partition: *
  // ===================================================

  virtual void initPartition()
  {
    return m_optimizer->initPartition();
  }

  virtual void moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules)
  {
    return m_optimizer->moveActiveNodesToPredefinedModules(modules);
  }

  virtual unsigned int optimizeActiveNetwork()
  {
    return m_optimizer->optimizeActiveNetwork();
  }

  virtual unsigned int tryMoveEachNodeIntoBestModule()
  {
    return m_optimizer->tryMoveEachNodeIntoBestModule();
  }

  virtual unsigned int tryMoveEachNodeIntoBestModuleInParallel()
  {
    return m_optimizer->tryMoveEachNodeIntoBestModuleInParallel();
  }

  virtual void consolidateModules(bool replaceExistingModules = true)
  {
    return m_optimizer->consolidateModules(replaceExistingModules);
  }

  virtual bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false)
  {
    return m_optimizer->restoreConsolidatedOptimizationPointIfNoImprovement(forceRestore);
  }

  // ===================================================
  // Debug: *
  // ===================================================

  virtual void printDebug()
  {
    return m_optimizer->printDebug();
  }

  // ===================================================
  // Protected members
  // ===================================================

  std::unique_ptr<InfomapOptimizerBase> m_optimizer;
};

} /* namespace infomap */

#endif // INFOMAP_CORE_H_
