/*
 * MetaMapEquation.h
 */

#include "MetaMapEquation.h"
#include "FlowData.h"
#include "InfoNode.h"
#include "../utils/Log.h"
#include "../io/Config.h"
#include <vector>
#include <set>
#include <map>
#include <utility>

namespace infomap {

double MetaMapEquation::getModuleCodelength() const
{
  // std::cout << "\n$$$$$ getModuleCodelength: " << moduleCodelength << " + " << metaCodelength << " = " << moduleCodelength + metaCodelength << "\n";
  return moduleCodelength + metaCodelength * metaDataRate;
}

double MetaMapEquation::getCodelength() const
{
  // std::cout << "\n$$$$$ getCodelength: " << codelength << " + " << metaCodelength << " = " << codelength + metaCodelength << "\n";
  return codelength + metaCodelength * metaDataRate;
}

// ===================================================
// IO
// ===================================================

std::ostream& MetaMapEquation::print(std::ostream& out) const
{
  return out << indexCodelength << " + " << moduleCodelength << " + " << metaCodelength << " = " << io::toPrecision(getCodelength());
}

std::ostream& operator<<(std::ostream& out, const MetaMapEquation& mapEq)
{
  return mapEq.print(out);
}


// ===================================================
// Init
// ===================================================

void MetaMapEquation::init(const Config& config)
{
  Log(3) << "MetaMapEquation::init()...\n";
  numMetaDataDimensions = config.numMetaDataDimensions;
  metaDataRate = config.metaDataRate;
  weightByFlow = !config.unweightedMetaData;
}


void MetaMapEquation::initNetwork(InfoNode& root)
{
  Log(3) << "MetaMapEquation::initNetwork()...\n";
  Base::initNetwork(root);
  // initMetaNodes(root);
  m_unweightedNodeFlow = 1.0 / root.childDegree();
}

void MetaMapEquation::initSuperNetwork(InfoNode& root)
{
  Base::initSuperNetwork(root);
}

void MetaMapEquation::initSubNetwork(InfoNode& root)
{
  Base::initSubNetwork(root);
}

void MetaMapEquation::initPartition(std::vector<InfoNode*>& nodes)
{
  initPartitionOfMetaNodes(nodes);

  calculateCodelength(nodes);
}

void MetaMapEquation::initMetaNodes(InfoNode& root)
{
  bool notInitiated = root.firstChild->metaCollection.empty();
  if (notInitiated) {
    Log(3) << "MetaMapEquation::initMetaNodesOnOriginalNetwork()...\n";
    for (InfoNode& node : root) {
      // Use only one meta dimension for now
      if (!node.metaData.empty()) {
        // std::cout << "\n@@@@@1 metaCollection.add(" << node.metaData[0] << ", " << (weightByFlow ? node.data.flow : 1) << ")\n";
        node.metaCollection.add(node.metaData[0], weightByFlow ? node.data.flow : m_unweightedNodeFlow);
        // std::cout << " -> " << node.metaCollection << "\n";
      } else
        throw std::length_error("A node is missing meta data using MetaMapEquation");
    }
  }
}

void MetaMapEquation::initPartitionOfMetaNodes(std::vector<InfoNode*>& nodes)
{
  Log(4) << "MetaMapEquation::initPartitionOfMetaNodes()...\n";
  m_moduleToMetaCollection.clear();

  for (auto& n : nodes) {
    InfoNode& node = *n;
    unsigned int moduleIndex = node.index; // Assume unique module index for all nodes in this initiation phase
    if (node.metaCollection.empty()) {
      if (!node.metaData.empty()) {
        double flow = weightByFlow ? node.data.flow : m_unweightedNodeFlow;
        // std::cout << "\n@@@@@2 metaCollection.add(" << node.metaData[0] << ", " << flow << ")\n";
        node.metaCollection.add(node.metaData[0], flow);
        // std::cout << "\n@@@@@2 -> " << node.metaCollection << "\n";
      } else
        throw std::length_error("A node is missing meta data using MetaMapEquation");
    }
    m_moduleToMetaCollection[moduleIndex] = node.metaCollection;
  }
}


// ===================================================
// Codelength
// ===================================================

void MetaMapEquation::calculateCodelength(std::vector<InfoNode*>& nodes)
{
  calculateCodelengthTerms(nodes);

  calculateCodelengthFromCodelengthTerms();

  metaCodelength = 0.0;

  // Treat each node as a single module
  for (InfoNode* n : nodes) {
    InfoNode& node = *n;
    metaCodelength += node.metaCollection.calculateEntropy();
  }

  // metaCodelength *= metaDataRate;

  // moduleCodelength += metaCodelength;
  // codelength += metaCodelength;

  // std::cout << "\n!!!!! calculateCodelength(nodes) -> meta: " << metaCodelength << "\n";
}

double MetaMapEquation::calcCodelength(const InfoNode& parent) const
{
  return parent.isLeafModule() ? calcCodelengthOnModuleOfLeafNodes(parent) :
                               // Use first-order model on index codebook
      MapEquation::calcCodelengthOnModuleOfModules(parent);
}

double MetaMapEquation::calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const
{
  double indexLength = MapEquation::calcCodelength(parent);

  // Meta addition
  MetaCollection metaCollection;
  for (const InfoNode& node : parent) {
    if (!node.metaCollection.empty())
      metaCollection.add(node.metaCollection);
    else
      metaCollection.add(node.metaData[0], weightByFlow ? node.data.flow : m_unweightedNodeFlow); // TODO: Initiate to collection and use all dimensions
  }

  double metaCodelength = metaCollection.calculateEntropy();

  // std::cout << "\n!!!!! calcCodelengthOnModuleOfLeafNodes(parent) -> meta: " << metaCodelength << "\n";

  return indexLength + metaDataRate * metaCodelength;
}


double MetaMapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
                                                       DeltaFlowDataType& oldModuleDelta,
                                                       DeltaFlowDataType& newModuleDelta,
                                                       std::vector<FlowDataType>& moduleFlowData,
                                                       std::vector<unsigned int>& moduleMembers)
{
  double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  double deltaMetaL = 0.0;

  unsigned int oldModuleIndex = oldModuleDelta.module;
  unsigned int newModuleIndex = newModuleDelta.module;

  // Remove codelength of old and new module before changes
  deltaMetaL -= getCurrentModuleMetaCodelength(oldModuleIndex, current, 0);
  deltaMetaL -= getCurrentModuleMetaCodelength(newModuleIndex, current, 0);
  // Add codelength of old module with current node removed
  deltaMetaL += getCurrentModuleMetaCodelength(oldModuleIndex, current, -1);
  // Add codelength of old module with current node added
  deltaMetaL += getCurrentModuleMetaCodelength(newModuleIndex, current, 1);

  // std::cout << "\n!!!!! getDeltaCodelengthOnMovingNode(" << current.metaCollection << ") from " <<
  // 	m_moduleToMetaCollection[oldModuleIndex] << " to " << m_moduleToMetaCollection[newModuleIndex] <<
  // 	" -> deltaMetaL: " << deltaMetaL << "\n";

  return deltaL + deltaMetaL * metaDataRate;
}


double MetaMapEquation::getCurrentModuleMetaCodelength(unsigned int module, InfoNode& current, int addRemoveOrNothing)
{
  auto& currentMetaCollection = m_moduleToMetaCollection[module];

  double moduleMetaCodelength = 0.0;

  if (addRemoveOrNothing == 0) {
    moduleMetaCodelength = currentMetaCollection.calculateEntropy();
  }
  // If add or remove, do the change, calculate new codelength and then undo the change
  else if (addRemoveOrNothing == 1) {
    currentMetaCollection.add(current.metaCollection);
    moduleMetaCodelength = currentMetaCollection.calculateEntropy();
    currentMetaCollection.remove(current.metaCollection);
  } else {
    currentMetaCollection.remove(current.metaCollection);
    moduleMetaCodelength = currentMetaCollection.calculateEntropy();
    currentMetaCollection.add(current.metaCollection);
  }

  // std::cout << "\n!!!!! getCurrentModuleMetaCodelength(module: " << module << ", node: " << current.stateId << ", meta: " << current.metaCollection << ", addRemove: " << addRemoveOrNothing << ") -> moduleMetaCodelength: " << moduleMetaCodelength << "\n";
  // std::cout << "  " << currentMetaCollection << "\n";

  return moduleMetaCodelength;
}


// ===================================================
// Consolidation
// ===================================================

void MetaMapEquation::updateCodelengthOnMovingNode(InfoNode& current,
                                                   DeltaFlowDataType& oldModuleDelta,
                                                   DeltaFlowDataType& newModuleDelta,
                                                   std::vector<FlowDataType>& moduleFlowData,
                                                   std::vector<unsigned int>& moduleMembers)
{
  Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  double deltaMetaL = 0.0;

  unsigned int oldModuleIndex = oldModuleDelta.module;
  unsigned int newModuleIndex = newModuleDelta.module;

  // Remove codelength of old and new module before changes
  deltaMetaL -= getCurrentModuleMetaCodelength(oldModuleIndex, current, 0);
  deltaMetaL -= getCurrentModuleMetaCodelength(newModuleIndex, current, 0);

  // Update meta data from moving node
  updateMetaData(current, oldModuleIndex, newModuleIndex);

  // Add codelength of old and new module after changes
  deltaMetaL += getCurrentModuleMetaCodelength(oldModuleIndex, current, 0);
  deltaMetaL += getCurrentModuleMetaCodelength(newModuleIndex, current, 0);

  // std::cout << "\n###### updateCodelengthOnMovingNode(), node: " << current.stateId <<
  // 	", meta: " << current.metaCollection << " -> deltaMetaL: " << deltaMetaL <<
  // 	", metaCodelength: " << metaCodelength << " -> " << (metaCodelength + deltaMetaL) << "\n";

  metaCodelength += deltaMetaL;

  // moduleCodelength += deltaMetaL;
  // codelength += deltaMetaL;
}


void MetaMapEquation::updateMetaData(InfoNode& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex)
{
  // Remove meta id from old module (can be a set of meta ids when moving submodules in coarse tune)
  auto& oldMetaCollection = m_moduleToMetaCollection[oldModuleIndex];
  oldMetaCollection.remove(current.metaCollection);

  // Add meta id to new module
  auto& newMetaCollection = m_moduleToMetaCollection[bestModuleIndex];
  newMetaCollection.add(current.metaCollection);
}


void MetaMapEquation::consolidateModules(std::vector<InfoNode*>& modules)
{
  for (auto& module : modules) {
    if (module == nullptr)
      continue;
    module->metaCollection = m_moduleToMetaCollection[module->index];
  }
}


// ===================================================
// Debug
// ===================================================

void MetaMapEquation::printDebug()
{
  std::cout << "MetaMapEquation\n";
  Base::printDebug();
}


} // namespace infomap
