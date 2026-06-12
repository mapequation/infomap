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

  // Compute each leaf node's share of the Markov entropy rate, p_alpha * h_alpha,
  // and its plogp(p_alpha), from edge weights. Undirected leaf networks store each
  // link once, so include both out- and in-edges (same pattern as the entropy rate
  // calculation in InfomapBase::generateSubNetwork). Accumulate the totals on the
  // root so they survive tree rewrites that bypass consolidateModules.
  root.lossyEntropy = 0.0;
  root.lossyFlowLogFlow = 0.0;
  for (InfoNode& node : root) {
    double sumWLogW = 0.0;
    double strength = 0.0;
    for (InfoEdge* e : node.outEdges()) {
      sumWLogW += infomath::plogp(e->data.weight);
      strength += e->data.weight;
    }
    for (InfoEdge* e : node.inEdges()) {
      sumWLogW += infomath::plogp(e->data.weight);
      strength += e->data.weight;
    }
    // h = -sum (w/s) log2 (w/s) = (plogp(s) - sum plogp(w)) / s
    double h = strength > 1e-16 ? (infomath::plogp(strength) - sumWLogW) / strength : 0.0;
    node.lossyEntropy = node.data.flow * h;
    node.lossyFlowLogFlow = infomath::plogp(node.data.flow);
    root.lossyEntropy += node.lossyEntropy;
    root.lossyFlowLogFlow += node.lossyFlowLogFlow;
  }
}

void LossyMapEquation::initPartition(std::vector<InfoNode*>& nodes)
{
  calculateCodelength(nodes);
}

// ===================================================
// Codelength
// ===================================================

double LossyMapEquation::calcLoss(double moduleFlow, double flowLogFlow) const
{
  return infomath::plogp(moduleFlow) - flowLogFlow;
}

double LossyMapEquation::calcCorrection(double moduleFlow, double flowLogFlow, double entropy) const
{
  return std::max(0.0, calcLoss(moduleFlow, flowLogFlow) - m_lambda * entropy);
}

void LossyMapEquation::calculateCodelength(std::vector<InfoNode*>& nodes)
{
  Base::calculateCodelength(nodes);

  // Rebuild per-module aggregates; at this point each active node is its own
  // module and node->index is its module id.
  auto numNodes = nodes.size();
  m_moduleEntropy.assign(numNodes, 0.0);
  m_moduleFlowLogFlow.assign(numNodes, 0.0);
  m_sumCorrection = 0.0;
  m_sumNoiseLoss = 0.0;
  m_sumNoiseEntropy = 0.0;

  for (InfoNode* n : nodes) {
    auto i = n->index;
    m_moduleEntropy[i] = n->lossyEntropy;
    m_moduleFlowLogFlow[i] = n->lossyFlowLogFlow;
    double c = calcCorrection(n->data.flow, n->lossyFlowLogFlow, n->lossyEntropy);
    if (c > 0.0) {
      m_sumCorrection += c;
      m_sumNoiseLoss += calcLoss(n->data.flow, n->lossyFlowLogFlow);
      m_sumNoiseEntropy += n->lossyEntropy;
    }
  }
}

double LossyMapEquation::calcCodelength(const InfoNode& parent) const
{
  double L = Base::calcCodelength(parent);

  // Subtract this module's correction, computed from leaf sums carried by the children.
  double flowLogFlow = 0.0;
  double entropy = 0.0;
  for (const auto& node : parent) {
    flowLogFlow += node.lossyFlowLogFlow;
    entropy += node.lossyEntropy;
  }
  return L - calcCorrection(parent.data.flow, flowLogFlow, entropy);
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
  unsigned int oldM = oldModuleDelta.module;
  unsigned int newM = newModuleDelta.module;

  auto removeModuleTerms = [&](unsigned int m) {
    double c = calcCorrection(moduleFlowData[m].flow, m_moduleFlowLogFlow[m], m_moduleEntropy[m]);
    if (c > 0.0) {
      m_sumCorrection -= c;
      m_sumNoiseLoss -= calcLoss(moduleFlowData[m].flow, m_moduleFlowLogFlow[m]);
      m_sumNoiseEntropy -= m_moduleEntropy[m];
    }
  };
  auto addModuleTerms = [&](unsigned int m) {
    double c = calcCorrection(moduleFlowData[m].flow, m_moduleFlowLogFlow[m], m_moduleEntropy[m]);
    if (c > 0.0) {
      m_sumCorrection += c;
      m_sumNoiseLoss += calcLoss(moduleFlowData[m].flow, m_moduleFlowLogFlow[m]);
      m_sumNoiseEntropy += m_moduleEntropy[m];
    }
  };

  removeModuleTerms(oldM);
  if (newM != oldM)
    removeModuleTerms(newM);

  Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  m_moduleEntropy[oldM] -= current.lossyEntropy;
  m_moduleEntropy[newM] += current.lossyEntropy;
  m_moduleFlowLogFlow[oldM] -= current.lossyFlowLogFlow;
  m_moduleFlowLogFlow[newM] += current.lossyFlowLogFlow;

  addModuleTerms(oldM);
  if (newM != oldM)
    addModuleTerms(newM);
}

void LossyMapEquation::consolidateModules(std::vector<InfoNode*>& modules)
{
  // Carry the leaf sums onto the new module nodes so the next aggregation level
  // and calcCodelength see them.
  for (unsigned int i = 0; i < modules.size(); ++i) {
    if (modules[i] == nullptr)
      continue;
    modules[i]->lossyEntropy = m_moduleEntropy[i];
    modules[i]->lossyFlowLogFlow = m_moduleFlowLogFlow[i];
  }
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
