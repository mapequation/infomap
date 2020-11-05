/*
 * InfoNode.h
 *
 *  Created on: 19 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_INFONODE_H_
#define SRC_CLUSTERING_INFONODE_H_

#include "FlowData.h"
#include "InfoEdge.h"
#include "infomapIterators.h"
#include "treeIterators.h"
#include "../utils/iterators.h"
#include "../utils/exceptions.h"
#include "../utils/MetaCollection.h"
#include <memory>
#include <iostream>
#include <vector>
#include <limits>

namespace infomap {

class InfomapBase;

class InfoNode {
public:
  using EdgeType = Edge<InfoNode>;

  using child_iterator = ChildIterator<InfoNode*>;
  using const_child_iterator = ChildIterator<InfoNode const*>;
  using infomap_child_iterator = InfomapChildIterator<InfoNode*>;
  using const_infomap_child_iterator = InfomapChildIterator<InfoNode const*>;

  using tree_iterator = TreeIterator<InfoNode*>;
  using const_tree_iterator = TreeIterator<InfoNode const*>;

  using leaf_node_iterator = LeafNodeIterator<InfoNode*>;
  using const_leaf_node_iterator = LeafNodeIterator<InfoNode const*>;
  using leaf_module_iterator = LeafModuleIterator<InfoNode*>;
  using const_leaf_module_iterator = LeafModuleIterator<InfoNode const*>;

  using post_depth_first_iterator = DepthFirstIterator<InfoNode*, false>;
  using const_post_depth_first_iterator = DepthFirstIterator<InfoNode const*, false>;

  using edge_iterator = std::vector<EdgeType*>::iterator;
  using const_edge_iterator = std::vector<EdgeType*>::const_iterator;

  using edge_iterator_wrapper = IterWrapper<edge_iterator>;
  using const_edge_iterator_wrapper = IterWrapper<const_edge_iterator>;

  using infomap_iterator_wrapper = IterWrapper<tree_iterator>;
  using const_infomap_iterator_wrapper = IterWrapper<const_tree_iterator>;

  using child_iterator_wrapper = IterWrapper<child_iterator>;
  using const_child_iterator_wrapper = IterWrapper<const_child_iterator>;

  using infomap_child_iterator_wrapper = IterWrapper<infomap_child_iterator>;
  using const_infomap_child_iterator_wrapper = IterWrapper<const_infomap_child_iterator>;

public:
  FlowData data;
  unsigned int index = 0; // Temporary index used in finding best module
  //	unsigned int originalIndex = 0; // Index in the original network (for leaf nodes)
  /*const*/ unsigned int stateId = 0; // Unique state node id for the leaf nodes
  /*const*/ unsigned int physicalId = 0; // Physical id equals stateId for first order networks, otherwise can be non-unique
  /*const*/ unsigned int layerId = 0; // Layer id for multilayer networks
  std::vector<int> metaData; // Categorical value for each meta data dimension

  InfoNode* owner = nullptr; // Infomap owner (if this is an Infomap root)
  InfoNode* parent = nullptr;
  InfoNode* previous = nullptr; // sibling
  InfoNode* next = nullptr; // sibling
  InfoNode* firstChild = nullptr;
  InfoNode* lastChild = nullptr;
  InfoNode* collapsedFirstChild = nullptr;
  InfoNode* collapsedLastChild = nullptr;
  double codelength = 0.0; //TODO: Better design for hierarchical stuff!?
  bool dirty = false;

  std::vector<PhysData> physicalNodes;
  MetaCollection metaCollection; // For modules
  std::vector<unsigned int> stateNodes; // For physically aggregated nodes

protected:
  //	SubStructure m_subStructure;
  // std::unique_ptr<InfomapBase> m_infomap;
  unsigned int m_childDegree = 0;
  bool m_childrenChanged = false;
  unsigned int m_numLeafMembers = 0;

  std::vector<EdgeType*> m_outEdges;
  std::vector<EdgeType*> m_inEdges;

  // std::unique_ptr<InfomapBase> m_infomap;
  InfomapBase* m_infomap = nullptr;

public:
  InfoNode(const FlowData& flowData)
      : data(flowData) {};

  // For first order nodes, physicalId equals stateId
  InfoNode(const FlowData& flowData, unsigned int stateId)
      : data(flowData), stateId(stateId), physicalId(stateId) {};

  InfoNode(const FlowData& flowData, unsigned int stateId, unsigned int physicalId)
      : data(flowData), stateId(stateId), physicalId(physicalId) {};

  InfoNode(const FlowData& flowData, unsigned int stateId, unsigned int physicalId, unsigned int layerId)
      : data(flowData), stateId(stateId), physicalId(physicalId), layerId(layerId) {};

  InfoNode() {};

  InfoNode(const InfoNode& other)
      : data(other.data),
        index(other.index),
        stateId(other.stateId),
        physicalId(other.physicalId),
        layerId(other.layerId),
        metaData(other.metaData),
        parent(other.parent),
        previous(other.previous),
        next(other.next),
        firstChild(other.firstChild),
        lastChild(other.lastChild),
        collapsedFirstChild(other.collapsedFirstChild),
        collapsedLastChild(other.collapsedLastChild),
        codelength(other.codelength),
        dirty(other.dirty),
        metaCollection(other.metaCollection),
        m_childDegree(other.m_childDegree),
        m_childrenChanged(other.m_childrenChanged),
        m_numLeafMembers(other.m_numLeafMembers) {}


  ~InfoNode();


  InfoNode& operator=(const InfoNode& other)
  {
    data = other.data;
    index = other.index;
    stateId = other.stateId;
    physicalId = other.physicalId;
    layerId = other.layerId;
    metaData = other.metaData;
    parent = other.parent;
    previous = other.previous;
    next = other.next;
    firstChild = other.firstChild;
    lastChild = other.lastChild;
    collapsedFirstChild = other.collapsedFirstChild;
    collapsedLastChild = other.collapsedLastChild;
    codelength = other.codelength;
    dirty = other.dirty;
    metaCollection = other.metaCollection;
    m_childDegree = other.m_childDegree;
    m_childrenChanged = other.m_childrenChanged;
    m_numLeafMembers = other.m_numLeafMembers;
    return *this;
  }

  // ---------------------------- Getters ----------------------------

  unsigned int getMetaData(unsigned int dimension = 0)
  {
    if (dimension >= metaData.size()) {
      return 0;
    }
    auto meta = metaData[dimension];
    return meta < 0 ? 0 : static_cast<unsigned int>(meta);
  }

  // ---------------------------- Infomap ----------------------------
  InfomapBase& getInfomap();

  InfomapBase& setInfomap(InfomapBase*);

  InfoNode* getInfomapRoot();

  InfoNode const* getInfomapRoot() const;

  /**
   * Dispose the Infomap instance if it exists
   * @return true if an existing Infomap instance was deleted
   */
  bool disposeInfomap();

  /**
   * Number of physical nodes in memory nodes
   */
  unsigned int numPhysicalNodes() const { return physicalNodes.size(); }

  // ---------------------------- Tree iterators ----------------------------

  // Default iteration on children
  child_iterator begin()
  {
    return child_iterator(this);
  }

  child_iterator end()
  {
    return child_iterator(nullptr);
  }

  const_child_iterator begin() const
  {
    return const_child_iterator(this);
  }

  const_child_iterator end() const
  {
    return const_child_iterator(nullptr);
  }

  child_iterator begin_child()
  {
    return child_iterator(this);
  }

  child_iterator end_child()
  {
    return child_iterator(nullptr);
  }

  const_child_iterator begin_child() const
  {
    return const_child_iterator(this);
  }

  const_child_iterator end_child() const
  {
    return const_child_iterator(nullptr);
  }

  child_iterator_wrapper children()
  {
    return child_iterator_wrapper(child_iterator(this), child_iterator(nullptr));
  }

  const_child_iterator_wrapper children() const
  {
    return const_child_iterator_wrapper(const_child_iterator(this), const_child_iterator(nullptr));
  }

  infomap_child_iterator_wrapper infomap_children()
  {
    return infomap_child_iterator_wrapper(infomap_child_iterator(this), infomap_child_iterator(nullptr));
  }

  const_infomap_child_iterator_wrapper infomap_children() const
  {
    return const_infomap_child_iterator_wrapper(const_infomap_child_iterator(this), const_infomap_child_iterator(nullptr));
  }

  // InfomapLeafIterator begin_leaf()
  // { return InfomapLeafIterator(this); }

  // InfomapLeafIterator end_leaf()
  // { return InfomapLeafIterator(nullptr); }

  post_depth_first_iterator begin_post_depth_first()
  {
    return post_depth_first_iterator(this);
  }

  leaf_node_iterator begin_leaf_nodes()
  {
    return leaf_node_iterator(this);
  }

  leaf_module_iterator begin_leaf_modules()
  {
    return leaf_module_iterator(this);
  }

  tree_iterator begin_tree(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max())
  {
    return tree_iterator(this, maxClusterLevel);
  }

  tree_iterator end_tree()
  {
    return tree_iterator(nullptr);
  }

  const_tree_iterator begin_tree(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max()) const
  {
    return const_tree_iterator(this, maxClusterLevel);
  }

  const_tree_iterator end_tree() const
  {
    return const_tree_iterator(nullptr);
  }

  infomap_iterator_wrapper infomapTree(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max())
  {
    return infomap_iterator_wrapper(tree_iterator(this, maxClusterLevel), tree_iterator(nullptr));
  }

  const_infomap_iterator_wrapper infomapTree(unsigned int maxClusterLevel = std::numeric_limits<unsigned int>::max()) const
  {
    return const_infomap_iterator_wrapper(const_tree_iterator(this, maxClusterLevel), const_tree_iterator(nullptr));
  }

  // ---------------------------- Graph iterators ----------------------------

  edge_iterator begin_outEdge()
  {
    return m_outEdges.begin();
  }

  edge_iterator end_outEdge()
  {
    return m_outEdges.end();
  }

  edge_iterator begin_inEdge()
  {
    return m_inEdges.begin();
  }

  edge_iterator end_inEdge()
  {
    return m_inEdges.end();
  }

  //	inout_edge_iterator begin_inoutEdge()
  //	{ return inout_edge_iterator(begin_inEdge(), end_inEdge(), begin_outEdge()); }
  //
  //	inout_edge_iterator	end_inoutEdge()
  //	{ return inout_edge_iterator(end_outEdge()); }

  edge_iterator_wrapper outEdges()
  {
    return edge_iterator_wrapper(m_outEdges);
  }

  edge_iterator_wrapper inEdges()
  {
    return edge_iterator_wrapper(m_inEdges);
  }

  // ---------------------------- Capacity ----------------------------

  unsigned int childDegree() const;

  bool isLeaf() const;
  bool isLeafModule() const;
  bool isRoot() const;

  unsigned int depth() const;

  unsigned int firstDepthBelow() const;

  unsigned int numLeafMembers()
  {
    return m_numLeafMembers;
  }

  bool isDangling()
  {
    return m_outEdges.empty();
  }

  unsigned int outDegree()
  {
    return m_outEdges.size();
  }

  unsigned int inDegree()
  {
    return m_inEdges.size();
  }

  unsigned int degree()
  {
    return outDegree() + inDegree();
  }

  //	InfomapBase* getSubInfomap()
  //	{ return m_subStructure.subInfomap.get(); }
  //
  //	const InfomapBase* getSubInfomap() const
  //	{ return m_subStructure.subInfomap.get(); }
  //
  //	SubStructure& getSubStructure()
  //	{ return m_subStructure; }
  //
  //	const SubStructure& getSubStructure() const
  //	{ return m_subStructure; }

  // ---------------------------- Order ----------------------------
  bool isFirst() const
  {
    return !parent || parent->firstChild == this;
  }

  bool isLast() const
  {
    return !parent || parent->lastChild == this;
  }

  unsigned int childIndex() const;

  // Generate 1-based tree path
  std::vector<unsigned int> calculatePath() const;

  unsigned int infomapChildDegree() const;

  unsigned int id() const { return stateId; }

  // ---------------------------- Operators ----------------------------

  bool operator==(const InfoNode& rhs) const
  {
    return this == &rhs;
  }

  bool operator!=(const InfoNode& rhs) const
  {
    return this != &rhs;
  }


  friend std::ostream& operator<<(std::ostream& out, const InfoNode& node)
  {
    if (node.isLeaf())
      out << "[" << node.physicalId << "]";
    else
      out << "[module]";
    return out;
  }

  // ---------------------------- Mutators ----------------------------

  /**
   * Clear a cloned node to initial state
   */
  void initClean();

  void sortChildrenOnFlow(bool recurse = true);

  /**
   * Release the children and store the child pointers for later expansion
   * @return the number of children collapsed
   */
  unsigned int collapseChildren();

  /**
   * Expand collapsed children
   * @return the number of collapsed children expanded
   */
  unsigned int expandChildren();

  // ------ OLD -----

  // After change, set the child degree if known instead of lazily computing it by traversing the linked list
  void setChildDegree(unsigned int value);

  void setNumLeafNodes(unsigned int value);

  void addChild(InfoNode* child);

  void releaseChildren();

  /**
   * If not already having a single child, replace children
   * with a single new node, assuming grandchildren.
   * @return the single child
   */
  InfoNode& replaceChildrenWithOneNode();

  /**
   * @return 1 if the node is removed, otherwise 0
   */
  unsigned int replaceWithChildren();

  void replaceWithChildrenDebug();

  // void storeModulesIn(InfoNode& other);
  // void restoreModulesTo(InfoNode& other);

  /**
   * @return The number of children removed
   */
  unsigned int replaceChildrenWithGrandChildren();

  void replaceChildrenWithGrandChildrenDebug();

  void remove(bool removeChildren);

  void deleteChildren();

  EdgeType* addOutEdge(InfoNode& target, double weight, double flow = 0.0)
  {
    EdgeType* edge = new EdgeType(*this, target, weight, flow);
    m_outEdges.push_back(edge);
    target.m_inEdges.push_back(edge);
    return edge;
  }

private:
  void calcChildDegree();
};

} // namespace infomap

#endif /* SRC_CLUSTERING_INFONODE_H_ */
