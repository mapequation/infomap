/*
 * MapEquation.h
 *
 *  Created on: 25 feb 2015
 *      Author: Daniel
 */
#include "MapEquation.h"
#include "../utils/infomath.h"
#include "../io/convert.h"
#include "../io/Config.h"
#include "../utils/Log.h"
#include "../utils/VectorMap.h"
#include "InfoNode.h"
#include "FlowData.h"
#include <vector>
#include <map>
#include <iostream>

namespace infomap {

// ===================================================
// Getters
// ===================================================


// ===================================================
// IO
// ===================================================

std::ostream& MapEquation::print(std::ostream& out) const
{
  return out << indexCodelength << " + " << moduleCodelength << " = " << io::toPrecision(codelength);
}

std::ostream& operator<<(std::ostream& out, const MapEquation& mapEq)
{
  return mapEq.print(out);
}


// ===================================================
// Init
// ===================================================

void MapEquation::init(const Config& config)
{
  Log(3) << "MapEquation::init()...\n";
}

void MapEquation::initNetwork(InfoNode& root)
{
  Log(3) << "MapEquation::initNetwork()...\n";

  nodeFlow_log_nodeFlow = 0.0;
  for (InfoNode& node : root) {
    nodeFlow_log_nodeFlow += infomath::plogp(node.data.flow);
  }
  initSubNetwork(root);
}

void MapEquation::initSuperNetwork(InfoNode& root)
{
  Log(3) << "MapEquation::initSuperNetwork()...\n";

  nodeFlow_log_nodeFlow = 0.0;
  for (InfoNode& node : root) {
    nodeFlow_log_nodeFlow += infomath::plogp(node.data.enterFlow);
  }
}

void MapEquation::initSubNetwork(InfoNode& root)
{
  exitNetworkFlow = root.data.exitFlow;
  exitNetworkFlow_log_exitNetworkFlow = infomath::plogp(exitNetworkFlow);
}

void MapEquation::initPartition(std::vector<InfoNode*>& nodes)
{
  calculateCodelength(nodes);
}


// ===================================================
// Codelength
// ===================================================

void MapEquation::calculateCodelength(std::vector<InfoNode*>& nodes)
{
  calculateCodelengthTerms(nodes);

  calculateCodelengthFromCodelengthTerms();
}

void MapEquation::calculateCodelengthTerms(std::vector<InfoNode*>& nodes)
{
  enter_log_enter = 0.0;
  flow_log_flow = 0.0;
  exit_log_exit = 0.0;
  enterFlow = 0.0;

  // For each module
  for (InfoNode* n : nodes) {
    InfoNode& node = *n;
    // own node/module codebook
    flow_log_flow += infomath::plogp(node.data.flow + node.data.exitFlow);

    // use of index codebook
    enter_log_enter += infomath::plogp(node.data.enterFlow);
    exit_log_exit += infomath::plogp(node.data.exitFlow);
    enterFlow += node.data.enterFlow;
  }
  enterFlow += exitNetworkFlow;
  enterFlow_log_enterFlow = infomath::plogp(enterFlow);
}

void MapEquation::calculateCodelengthFromCodelengthTerms()
{
  indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
  moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
  codelength = indexCodelength + moduleCodelength;
}

double MapEquation::calcCodelength(const InfoNode& parent) const
{
  return parent.isLeafModule() ? calcCodelengthOnModuleOfLeafNodes(parent) : calcCodelengthOnModuleOfModules(parent);
}

double MapEquation::calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const
{
  double parentFlow = parent.data.flow;
  double parentExit = parent.data.exitFlow;
  double totalParentFlow = parentFlow + parentExit;
  if (totalParentFlow < 1e-16)
    return 0.0;

  double indexLength = 0.0;
  for (const auto& node : parent) {
    indexLength -= infomath::plogp(node.data.flow / totalParentFlow);
  }
  indexLength -= infomath::plogp(parentExit / totalParentFlow);

  indexLength *= totalParentFlow;

  return indexLength;
}

double MapEquation::calcCodelengthOnModuleOfModules(const InfoNode& parent) const
{
  double parentFlow = parent.data.flow;
  double parentExit = parent.data.exitFlow;
  if (parentFlow < 1e-16)
    return 0.0;

  // H(x) = -xlog(x), T = q + SUM(p), q = exitFlow, p = enterFlow
  // Normal format
  // L = q * -log(q/T) + SUM(p * -log(p/T))
  // Compact format
  // L = T * ( H(q/T) + SUM( H(p/T) ) )
  // Expanded format
  // L = q * -log(q) - q * -log(T) + SUM( p * -log(p) - p * -log(T) )
  // = T * log(T) - q*log(q) - SUM( p*log(p) )
  // = -H(T) + H(q) + SUM(H(p))
  // As T is not known, use expanded format to avoid two loops
  double sumEnter = 0.0;
  double sumEnterLogEnter = 0.0;
  for (const auto& node : parent) {
    sumEnter += node.data.enterFlow; // rate of enter to finer level
    sumEnterLogEnter += infomath::plogp(node.data.enterFlow);
  }
  // The possibilities from this module: Either exit to coarser level or enter one of its children
  double totalCodewordUse = parentExit + sumEnter;

  return infomath::plogp(totalCodewordUse) - sumEnterLogEnter - infomath::plogp(parentExit);
}


double MapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
                                                   DeltaFlowDataType& oldModuleDelta,
                                                   DeltaFlowDataType& newModuleDelta,
                                                   std::vector<FlowDataType>& moduleFlowData,
                                                   std::vector<unsigned int>& moduleMembers)
{
  using infomath::plogp;
  unsigned int oldModule = oldModuleDelta.module;
  unsigned int newModule = newModuleDelta.module;
  double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
  double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

  double delta_enter = plogp(enterFlow + deltaEnterExitOldModule - deltaEnterExitNewModule) - enterFlow_log_enterFlow;

  double delta_enter_log_enter = -plogp(moduleFlowData[oldModule].enterFlow)
      - plogp(moduleFlowData[newModule].enterFlow)
      + plogp(moduleFlowData[oldModule].enterFlow - current.data.enterFlow + deltaEnterExitOldModule)
      + plogp(moduleFlowData[newModule].enterFlow + current.data.enterFlow - deltaEnterExitNewModule);

  double delta_exit_log_exit = -plogp(moduleFlowData[oldModule].exitFlow)
      - plogp(moduleFlowData[newModule].exitFlow)
      + plogp(moduleFlowData[oldModule].exitFlow - current.data.exitFlow + deltaEnterExitOldModule)
      + plogp(moduleFlowData[newModule].exitFlow + current.data.exitFlow - deltaEnterExitNewModule);

  double delta_flow_log_flow = -plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow)
      - plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow)
      + plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow
              - current.data.exitFlow - current.data.flow + deltaEnterExitOldModule)
      + plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow
              + current.data.exitFlow + current.data.flow - deltaEnterExitNewModule);

  double deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
  return deltaL;
}

void MapEquation::updateCodelengthOnMovingNode(InfoNode& current,
                                               DeltaFlowDataType& oldModuleDelta,
                                               DeltaFlowDataType& newModuleDelta,
                                               std::vector<FlowDataType>& moduleFlowData,
                                               std::vector<unsigned int>& moduleMembers)
{
  using infomath::plogp;
  unsigned int oldModule = oldModuleDelta.module;
  unsigned int newModule = newModuleDelta.module;
  double deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
  double deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

  enterFlow -= moduleFlowData[oldModule].enterFlow + moduleFlowData[newModule].enterFlow;
  enter_log_enter -= plogp(moduleFlowData[oldModule].enterFlow) + plogp(moduleFlowData[newModule].enterFlow);
  exit_log_exit -= plogp(moduleFlowData[oldModule].exitFlow) + plogp(moduleFlowData[newModule].exitFlow);
  flow_log_flow -= plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);


  moduleFlowData[oldModule] -= current.data;
  moduleFlowData[newModule] += current.data;

  moduleFlowData[oldModule].enterFlow += deltaEnterExitOldModule;
  moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
  moduleFlowData[newModule].enterFlow -= deltaEnterExitNewModule;
  moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;

  enterFlow += moduleFlowData[oldModule].enterFlow + moduleFlowData[newModule].enterFlow;
  enter_log_enter += plogp(moduleFlowData[oldModule].enterFlow) + plogp(moduleFlowData[newModule].enterFlow);
  exit_log_exit += plogp(moduleFlowData[oldModule].exitFlow) + plogp(moduleFlowData[newModule].exitFlow);
  flow_log_flow += plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);

  enterFlow_log_enterFlow = plogp(enterFlow);

  indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
  moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
  codelength = indexCodelength + moduleCodelength;
}


// ===================================================
// Debug
// ===================================================

void MapEquation::printDebug()
{
  std::cout << "(enterFlow_log_enterFlow: " << enterFlow_log_enterFlow << ", "
            << "enter_log_enter: " << enter_log_enter << ", "
            << "exitNetworkFlow_log_exitNetworkFlow: " << exitNetworkFlow_log_exitNetworkFlow << ") ";
  //	std::cout << "enterFlow_log_enterFlow: " << enterFlow_log_enterFlow << "\n" <<
  //			"enter_log_enter: " << enter_log_enter << "\n" <<
  //			"exitNetworkFlow_log_exitNetworkFlow: " << exitNetworkFlow_log_exitNetworkFlow << "\n";
  //	std::cout << "exit_log_exit: " << exit_log_exit << "\n" <<
  //			"flow_log_flow: " << flow_log_flow << "\n" <<
  //			"nodeFlow_log_nodeFlow: " << nodeFlow_log_nodeFlow << "\n";
}

} // namespace infomap
