/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#if INFOMAP_FEATURE_LOSSY_MAP_EQUATION

#include "LossyMapEquation.h"
#include "InfoEdge.h"
#include "InfoNode.h"
#include "../utils/Log.h"

#include <algorithm>
#include <vector>

namespace infomap {

// ===================================================
// IO
// ===================================================

std::ostream& LossyMapEquation::print(std::ostream& out) const
{
  return out << indexCodelength << " + " << (moduleCodelength - m_sumCorrection)
             << " = " << io::toPrecision(getCodelength());
}

std::ostream& operator<<(std::ostream& out, const LossyMapEquation& mapEq)
{
  return mapEq.print(out);
}

// ===================================================
// Init
// ===================================================

void LossyMapEquation::init(const Config& config)
{
  Log(3) << "LossyMapEquation::init()...\n";
  Base::init(config);
  m_lambda = config.lossyLambda;
}

void LossyMapEquation::initNetwork(InfoNode& root)
{
  Base::initNetwork(root);
}

void LossyMapEquation::initPartition(std::vector<InfoNode*>& nodes)
{
  calculateCodelength(nodes);
}

// ===================================================
// Codelength
// ===================================================

double LossyMapEquation::calcLoss(double /*moduleFlow*/, double /*flowLogFlow*/) const
{
  return 0.0;
}

double LossyMapEquation::calcCorrection(double /*moduleFlow*/, double /*flowLogFlow*/, double /*entropy*/) const
{
  return 0.0;
}

void LossyMapEquation::calculateCodelength(std::vector<InfoNode*>& nodes)
{
  Base::calculateCodelength(nodes);
}

double LossyMapEquation::calcCodelength(const InfoNode& parent) const
{
  return Base::calcCodelength(parent);
}

double LossyMapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
                                                        DeltaFlow& oldModuleDelta,
                                                        DeltaFlow& newModuleDelta,
                                                        std::vector<FlowData>& moduleFlowData,
                                                        std::vector<unsigned int>& moduleMembers)
{
  return Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);
}

// ===================================================
// Consolidation
// ===================================================

void LossyMapEquation::updateCodelengthOnMovingNode(InfoNode& current,
                                                    DeltaFlow& oldModuleDelta,
                                                    DeltaFlow& newModuleDelta,
                                                    std::vector<FlowData>& moduleFlowData,
                                                    std::vector<unsigned int>& moduleMembers)
{
  Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);
}

void LossyMapEquation::consolidateModules(std::vector<InfoNode*>& /*modules*/)
{
}

// ===================================================
// Debug
// ===================================================

void LossyMapEquation::printDebug() const
{
  Log() << "LossyMapEquation\n";
  Base::printDebug();
}

} // namespace infomap

#endif // INFOMAP_FEATURE_LOSSY_MAP_EQUATION
