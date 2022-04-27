/*******************************************************************************
  Infomap software package for multi-level network clustering

  Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

  For more information, see <http://www.mapequation.org>

  This file is part of Infomap software package.

  Infomap software package is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Infomap software package is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.
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
  using OptimizerPtr = std::unique_ptr<InfomapOptimizerBase>;

protected:
public:
  InfomapCore() : InfomapBase() { initOptimizer(); }
  InfomapCore(const std::string flags) : InfomapBase(flags) { initOptimizer(); }
  InfomapCore(const Config& conf) : InfomapBase(conf) { initOptimizer(); }
  virtual ~InfomapCore() { }

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
      m_optimizer = OptimizerPtr(new InfomapOptimizer<MetaMapEquation>());
    } else if (haveMemory() && !forceNoMemory) {
      m_optimizer = OptimizerPtr(new InfomapOptimizer<MemMapEquation>());
    } else {
      m_optimizer = OptimizerPtr(new InfomapOptimizer<BiasedMapEquation>());
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

  // unsigned int tryMoveEachNodeIntoBestModuleLocal() {
  // }

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

  OptimizerPtr m_optimizer;
};

} /* namespace infomap */

#endif /* INFOMAP_CORE_H_ */
