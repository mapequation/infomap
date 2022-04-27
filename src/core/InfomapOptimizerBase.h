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

#ifndef INFOMAP_OPTIMIZER_BASE_H_
#define INFOMAP_OPTIMIZER_BASE_H_

#include "InfomapBase.h"
#include "InfoNode.h"
#include "FlowData.h"
#include <vector>

namespace infomap {

class InfomapOptimizerBase {
  friend class InfomapCore;
  using FlowDataType = FlowData;

public:
  InfomapOptimizerBase() = default;

  virtual ~InfomapOptimizerBase() = default;

  virtual void init(InfomapBase* infomap) = 0;

  // ===================================================
  // IO
  // ===================================================

  virtual std::ostream& toString(std::ostream& out) const = 0;

  // ===================================================
  // Getters
  // ===================================================

  virtual double getCodelength() const = 0;

  virtual double getIndexCodelength() const = 0;

  virtual double getModuleCodelength() const = 0;

  virtual double getMetaCodelength(bool unweighted = false) const { return 0.0; }

protected:
  virtual unsigned int numActiveModules() const = 0;

  // ===================================================
  // Run: Init: *
  // ===================================================

  // Init terms that is constant for the whole network
  virtual void initTree() = 0;

  virtual void initNetwork() = 0;

  virtual void initSuperNetwork() = 0;

  virtual double calcCodelength(const InfoNode& parent) const = 0;

  // ===================================================
  // Run: Partition: *
  // ===================================================

  virtual void initPartition() = 0;

  virtual void moveActiveNodesToPredefinedModules(std::vector<unsigned int>& modules) = 0;

  virtual unsigned int optimizeActiveNetwork() = 0;

  virtual unsigned int tryMoveEachNodeIntoBestModule() = 0;

  // virtual unsigned int tryMoveEachNodeIntoBestModuleLocal() = 0;

  virtual unsigned int tryMoveEachNodeIntoBestModuleInParallel() = 0;

  virtual void consolidateModules(bool replaceExistingModules = true) = 0;

  virtual bool restoreConsolidatedOptimizationPointIfNoImprovement(bool forceRestore = false) = 0;

  // ===================================================
  // Debug: *
  // ===================================================

  virtual void printDebug() = 0;
};

} /* namespace infomap */

#endif /* INFOMAP_OPTIMIZER_BASE_H_ */
