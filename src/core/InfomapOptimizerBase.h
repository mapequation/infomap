/**
 * 
 */

#ifndef _INFOMAPOPTIMIZERBASE_H_
#define _INFOMAPOPTIMIZERBASE_H_

#include "InfomapBase.h"
#include "InfoNode.h"
#include "FlowData.h"
#include <vector>

namespace infomap {

class InfomapOptimizerBase {
  friend class InfomapCore;

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

  virtual bool haveMemory() const = 0;

protected:
  virtual unsigned int numActiveModules() const = 0;

  // ===================================================
  // Run: Init: *
  // ===================================================

  // Init terms that is constant for the whole network
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

  virtual void printDebug() const = 0;
};

} /* namespace infomap */

#endif /* _INFOMAPOPTIMIZERBASE_H_ */
