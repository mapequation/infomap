/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "OutputView.h"
#include "../core/InfomapBase.h"
#include "../core/InfoNode.h"
#include "../core/StateNetwork.h"
#include "../utils/convert.h"

namespace infomap {

OutputView::OutputView(InfomapBase& infomap, const StateNetwork& network, bool states)
    : m_infomap(infomap), m_network(network), m_states(states) {}

bool OutputView::isHigherOrderPhysicalLevel() const
{
  return m_infomap.haveMemory() && !m_states;
}

bool OutputView::isMultilayer() const
{
  return m_infomap.isMultilayerNetwork();
}

bool OutputView::hasMetaData() const
{
  return m_infomap.haveMetaData();
}

void OutputView::forEachLeaf(int moduleIndexLevel, OutputLeafFilter filter, const LeafCallback& callback)
{
  if (isHigherOrderPhysicalLevel()) {
    for (auto it(m_infomap.iterTreePhysical(moduleIndexLevel)); !it.isEnd(); ++it) {
      InfoNode& node = *it;
      if (node.isLeaf() && shouldIncludeLeaf(node, filter)) {
        callback(leafRow(node, it.path(), it.moduleId(), it.modularCentrality()));
      }
    }
    return;
  }

  for (auto it(m_infomap.iterTree(moduleIndexLevel)); !it.isEnd(); ++it) {
    InfoNode& node = *it;
    if (node.isLeaf() && shouldIncludeLeaf(node, filter)) {
      callback(leafRow(node, it.path(), it.moduleId(), it.modularCentrality()));
    }
  }
}

void OutputView::forEachTreeNode(const TreeCallback& callback)
{
  if (isHigherOrderPhysicalLevel()) {
    for (auto it(m_infomap.iterTreePhysical()); !it.isEnd(); ++it) {
      callback(treeRow(*it, it.depth()));
    }
    return;
  }

  for (auto it(m_infomap.iterTree()); !it.isEnd(); ++it) {
    callback(treeRow(*it, it.depth()));
  }
}

std::map<unsigned int, OutputStateNodeTarget> OutputView::stateNodeTargets()
{
  std::map<unsigned int, OutputStateNodeTarget> targets;

  if (isHigherOrderPhysicalLevel()) {
    for (auto it(m_infomap.iterTreePhysical()); !it.isEnd(); ++it) {
      if (it->isLeaf()) {
        for (auto stateId : it->stateNodes) {
          targets[stateId] = { it->parent, it.childIndex() };
        }
      }
    }
    return targets;
  }

  for (auto it(m_infomap.iterTree()); !it.isEnd(); ++it) {
    if (it->isLeaf()) {
      targets[it->stateId] = { it->parent, it.childIndex() };
    }
  }

  return targets;
}

bool OutputView::shouldIncludeLeaf(const InfoNode& node, OutputLeafFilter filter) const
{
  const auto shouldHideBipartiteNodes = filter == OutputLeafFilter::TreeNoFlowFilter
      ? !m_infomap.printFlowTree && m_infomap.isBipartite() && m_infomap.hideBipartiteNodes
      : m_infomap.isBipartite() && m_infomap.hideBipartiteNodes;

  return !shouldHideBipartiteNodes || node.physicalId < m_network.bipartiteStartId();
}

OutputLeafRow OutputView::leafRow(const InfoNode& node,
                                  const std::deque<unsigned int>& path,
                                  unsigned int moduleId,
                                  double modularCentrality) const
{
  return { node, path, moduleId, modularCentrality, node.stateId, node.physicalId, node.layerId, node.data.flow, nodeName(node) };
}

OutputTreeRow OutputView::treeRow(const InfoNode& node, unsigned int depth) const
{
  return { node, depth, node.stateId, node.physicalId, node.data.flow };
}

std::string OutputView::nodeName(const InfoNode& node) const
{
  try {
    return m_network.names().at(node.physicalId);
  } catch (...) {
    return io::stringify(node.physicalId);
  }
}

} // namespace infomap
