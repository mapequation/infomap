/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef OUTPUT_VIEW_H_
#define OUTPUT_VIEW_H_

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>

namespace infomap {

class InfomapBase;
class InfoNode;
class StateNetwork;

enum class OutputLeafFilter : std::uint8_t {
  Regular,
  TreeNoFlowFilter
};

struct OutputLeafRow {
  const InfoNode& node;
  const std::deque<unsigned int>& path;
  unsigned int moduleId = 0;
  double modularCentrality = 0.0;
  unsigned int stateId = 0;
  unsigned int physicalId = 0;
  unsigned int layerId = 0;
  double flow = 0.0;
  std::string name;
};

struct OutputTreeRow {
  const InfoNode& node;
  unsigned int depth = 0;
  unsigned int stateId = 0;
  unsigned int physicalId = 0;
  double flow = 0.0;
};

struct OutputStateNodeTarget {
  InfoNode* parent = nullptr;
  unsigned int childIndex = 0;
};

class OutputView {
public:
  using LeafCallback = std::function<void(const OutputLeafRow&)>;
  using TreeCallback = std::function<void(const OutputTreeRow&)>;

  OutputView(InfomapBase& infomap, const StateNetwork& network, bool states);

  bool isStateLevel() const { return m_states; }
  bool isPhysicalLevel() const { return !m_states; }
  bool isHigherOrderPhysicalLevel() const;
  bool isMultilayer() const;
  bool hasMetaData() const;

  void forEachLeaf(int moduleIndexLevel, OutputLeafFilter filter, const LeafCallback& callback);
  void forEachTreeNode(const TreeCallback& callback);

  std::map<unsigned int, OutputStateNodeTarget> stateNodeTargets();

private:
  bool shouldIncludeLeaf(const InfoNode& node, OutputLeafFilter filter) const;
  OutputLeafRow leafRow(const InfoNode& node,
                        const std::deque<unsigned int>& path,
                        unsigned int moduleId,
                        double modularCentrality) const;
  OutputTreeRow treeRow(const InfoNode& node, unsigned int depth) const;
  std::string nodeName(const InfoNode& node) const;

  InfomapBase& m_infomap;
  const StateNetwork& m_network;
  bool m_states = false;
};

} // namespace infomap

#endif // OUTPUT_VIEW_H_
