/*
 * MetaMapEquation.h
 */

#include "MetaMapEquation.h"
#include "FlowData.h"
#include "InfoNode.h"
#include "../utils/Log.h"
#include "../io/convert.h"
#include "../io/Config.h"
#include <vector>
#include <set>
#include <utility>

namespace infomap {

double MetaMapEquation::getModuleCodelength() const noexcept
{
  return moduleCodelength + metaCodelength * metaDataRate;
}

double MetaMapEquation::getCodelength() const noexcept
{
  return codelength + metaCodelength * metaDataRate;
}

// ===================================================
// IO
// ===================================================

std::ostream& MetaMapEquation::print(std::ostream& out) const noexcept
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

void MetaMapEquation::init(const Config& config) noexcept
{
  Log(3) << "MetaMapEquation::init()...\n";
  numMetaDataDimensions = config.numMetaDataDimensions;
  metaDataRate = config.metaDataRate;
  weightByFlow = !config.unweightedMetaData;
}


void MetaMapEquation::initNetwork(InfoNode& root) noexcept
{
  Log(3) << "MetaMapEquation::initNetwork()...\n";
  Base::initNetwork(root);
  m_unweightedNodeFlow = 1.0 / root.childDegree();
}

void MetaMapEquation::initPartition(std::vector<InfoNode*>& nodes) noexcept
{
  initPartitionOfMetaNodes(nodes);

  calculateCodelength(nodes);
}

void MetaMapEquation::initMetaNodes(InfoNode& root)
{
  const bool notInitiated = root.firstChild->metaCollection.empty();

  if (notInitiated) {
    Log(3) << "MetaMapEquation::initMetaNodesOnOriginalNetwork()...\n";
    for (auto& node : root) {
      // Use only one meta dimension for now
      if (node.metaData.empty()) {
        throw std::length_error("A node is missing meta data using MetaMapEquation");
      }

      node.metaCollection.add(node.metaData[0], weightByFlow ? node.data.flow : m_unweightedNodeFlow);
    }
  }
}

void MetaMapEquation::initPartitionOfMetaNodes(std::vector<InfoNode*>& nodes)
{
  Log(4) << "MetaMapEquation::initPartitionOfMetaNodes()...\n";
  m_moduleToMetaCollection.clear();

  for (auto& n : nodes) {
    auto& node = *n;
    const unsigned int moduleIndex = node.index; // Assume unique module index for all nodes in this initiation phase

    if (node.metaCollection.empty()) {
      if (!node.metaData.empty()) {
        double flow = weightByFlow ? node.data.flow : m_unweightedNodeFlow;
        node.metaCollection.add(node.metaData[0], flow);
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
  for (auto* n : nodes) {
    auto& node = *n;
    metaCodelength += node.metaCollection.calculateEntropy();
  }
}

double MetaMapEquation::calcCodelength(const InfoNode& parent) const
{
  return parent.isLeafModule() ? calcCodelengthOnModuleOfLeafNodes(parent)
                               : MapEquation::calcCodelengthOnModuleOfModules(parent);
}

double MetaMapEquation::calcCodelengthOnModuleOfLeafNodes(const InfoNode& parent) const
{
  const double indexLength = MapEquation::calcCodelength(parent);

  // Meta addition
  auto metaCollection = MetaCollection();

  for (const auto& node : parent) {
    if (!node.metaCollection.empty())
      metaCollection.add(node.metaCollection);
    else
      metaCollection.add(node.metaData[0], weightByFlow ? node.data.flow : m_unweightedNodeFlow); // TODO: Initiate to collection and use all dimensions
  }

  const double metaCodelength_ = metaCollection.calculateEntropy();

  return indexLength + metaDataRate * metaCodelength_;
}


double MetaMapEquation::getDeltaCodelengthOnMovingNode(InfoNode& current,
                                                       DeltaFlow& oldModuleDelta,
                                                       DeltaFlow& newModuleDelta,
                                                       std::vector<FlowData>& moduleFlowData,
                                                       std::vector<unsigned int>& moduleMembers) const
{
  const double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  double deltaMetaL = 0.0;

  const unsigned int oldModuleIndex = oldModuleDelta.module;
  const unsigned int newModuleIndex = newModuleDelta.module;

  // Remove codelength of old and new module before changes
  deltaMetaL -= getCurrentModuleMetaCodelength(oldModuleIndex, current, 0);
  deltaMetaL -= getCurrentModuleMetaCodelength(newModuleIndex, current, 0);
  // Add codelength of old module with current node removed
  deltaMetaL += getCurrentModuleMetaCodelength(oldModuleIndex, current, -1);
  // Add codelength of old module with current node added
  deltaMetaL += getCurrentModuleMetaCodelength(newModuleIndex, current, 1);

  return deltaL + deltaMetaL * metaDataRate;
}


double MetaMapEquation::getCurrentModuleMetaCodelength(unsigned int module, InfoNode& current, int addRemoveOrNothing) const
{
  auto& currentMetaCollection = m_moduleToMetaCollection.at(module);

  double moduleMetaCodelength;

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

  return moduleMetaCodelength;
}


// ===================================================
// Consolidation
// ===================================================

void MetaMapEquation::updateCodelengthOnMovingNode(InfoNode& current,
                                                   DeltaFlow& oldModuleDelta,
                                                   DeltaFlow& newModuleDelta,
                                                   std::vector<FlowData>& moduleFlowData,
                                                   std::vector<unsigned int>& moduleMembers)
{
  Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

  double deltaMetaL = 0.0;

  const unsigned int oldModuleIndex = oldModuleDelta.module;
  const unsigned int newModuleIndex = newModuleDelta.module;

  // Remove codelength of old and new module before changes
  deltaMetaL -= getCurrentModuleMetaCodelength(oldModuleIndex, current, 0);
  deltaMetaL -= getCurrentModuleMetaCodelength(newModuleIndex, current, 0);

  // Update meta data from moving node
  updateMetaData(current, oldModuleIndex, newModuleIndex);

  // Add codelength of old and new module after changes
  deltaMetaL += getCurrentModuleMetaCodelength(oldModuleIndex, current, 0);
  deltaMetaL += getCurrentModuleMetaCodelength(newModuleIndex, current, 0);

  metaCodelength += deltaMetaL;
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

void MetaMapEquation::printDebug() const
{
  std::cout << "MetaMapEquation\n";
  Base::printDebug();
}


} // namespace infomap
