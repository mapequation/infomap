/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef FLOWDATA_H_
#define FLOWDATA_H_

#include <ostream>
#include <utility>
#include <map>
#include "StateNetwork.h"

namespace infomap {

struct FlowData {
  double flow = 0.0;
  double enterFlow = 0.0;
  double exitFlow = 0.0;
  double teleportFlow = 0.0;
  double teleportWeight = 0.0;
  double teleportSourceFlow = 0.0; // TODO: Skip?
  double danglingFlow = 0.0; // TODO: Skip?

  FlowData() = default;
  FlowData(double flow) : flow(flow) { }
  virtual ~FlowData() = default;

  virtual void init(const StateNetwork::StateNode& node)
  {
    teleportWeight = node.weight;
    teleportFlow = node.teleFlow;
    exitFlow = node.exitFlow;
    enterFlow = node.enterFlow;
  }

  FlowData& operator+=(const FlowData& other)
  {
    flow += other.flow;
    enterFlow += other.enterFlow;
    exitFlow += other.exitFlow;
    teleportFlow += other.teleportFlow;
    teleportSourceFlow += other.teleportSourceFlow;
    teleportWeight += other.teleportWeight;
    danglingFlow += other.danglingFlow;
    return *this;
  }

  FlowData& operator-=(const FlowData& other)
  {
    flow -= other.flow;
    enterFlow -= other.enterFlow;
    exitFlow -= other.exitFlow;
    teleportFlow -= other.teleportFlow;
    teleportSourceFlow -= other.teleportSourceFlow;
    teleportWeight -= other.teleportWeight;
    danglingFlow -= other.danglingFlow;
    return *this;
  }

  friend std::ostream& operator<<(std::ostream& out, const FlowData& data)
  {
    return out << "flow: " << data.flow << ", enter: " << data.enterFlow << ", exit: " << data.exitFlow
               << ", teleWeight: " << data.teleportWeight << ", danglingFlow: " << data.danglingFlow
               << ", teleFlow: " << data.teleportFlow;
  }
};

struct DeltaFlow {
  unsigned int module = 0;
  double deltaExit = 0.0;
  double deltaEnter = 0.0;
  unsigned int count = 0;

  explicit DeltaFlow(unsigned int module, double deltaExit, double deltaEnter)
      : module(module),
        deltaExit(deltaExit),
        deltaEnter(deltaEnter) { }

  DeltaFlow() = default;
  DeltaFlow(const DeltaFlow&) = default;
  DeltaFlow(DeltaFlow&&) = default;
  DeltaFlow& operator=(const DeltaFlow&) = default;
  DeltaFlow& operator=(DeltaFlow&&) = default;
  virtual ~DeltaFlow() = default;

  DeltaFlow& operator+=(const DeltaFlow& other)
  {
    module = other.module;
    deltaExit += other.deltaExit;
    deltaEnter += other.deltaEnter;
    ++count;
    return *this;
  }

  virtual void reset()
  {
    module = 0;
    deltaExit = 0.0;
    deltaEnter = 0.0;
    count = 0;
  }

  friend void swap(DeltaFlow& first, DeltaFlow& second) noexcept
  {
    std::swap(first.module, second.module);
    std::swap(first.deltaExit, second.deltaExit);
    std::swap(first.deltaEnter, second.deltaEnter);
    std::swap(first.count, second.count);
  }

  friend std::ostream& operator<<(std::ostream& out, const DeltaFlow& data)
  {
    return out << "module: " << data.module << ", deltaEnter: " << data.deltaEnter << ", deltaExit: " << data.deltaExit << ", count: " << data.count;
  }
};

struct MemDeltaFlow : DeltaFlow {
  double sumDeltaPlogpPhysFlow = 0.0;
  double sumPlogpPhysFlow = 0.0;

  MemDeltaFlow() = default;

  explicit MemDeltaFlow(unsigned int module, double deltaExit, double deltaEnter, double sumDeltaPlogpPhysFlow = 0.0, double sumPlogpPhysFlow = 0.0)
      : DeltaFlow(module, deltaExit, deltaEnter),
        sumDeltaPlogpPhysFlow(sumDeltaPlogpPhysFlow),
        sumPlogpPhysFlow(sumPlogpPhysFlow) { }

  MemDeltaFlow& operator+=(const MemDeltaFlow& other)
  {
    DeltaFlow::operator+=(other);
    sumDeltaPlogpPhysFlow += other.sumDeltaPlogpPhysFlow;
    sumPlogpPhysFlow += other.sumPlogpPhysFlow;
    return *this;
  }

  void reset() override
  {
    DeltaFlow::reset();
    sumDeltaPlogpPhysFlow = 0.0;
    sumPlogpPhysFlow = 0.0;
  }

  friend void swap(MemDeltaFlow& first, MemDeltaFlow& second) noexcept
  {
    swap(static_cast<DeltaFlow&>(first), static_cast<DeltaFlow&>(second));
    std::swap(first.sumDeltaPlogpPhysFlow, second.sumDeltaPlogpPhysFlow);
    std::swap(first.sumPlogpPhysFlow, second.sumPlogpPhysFlow);
  }

  friend std::ostream& operator<<(std::ostream& out, const MemDeltaFlow& data)
  {
    return out << "module: " << data.module << ", deltaEnter: " << data.deltaEnter << ", deltaExit: " << data.deltaExit << ", count: " << data.count << ", sumDeltaPlogpPhysFlow: " << data.sumDeltaPlogpPhysFlow << ", sumPlogpPhysFlow: " << data.sumPlogpPhysFlow;
  }
};

struct PhysData {
  unsigned int physNodeIndex;
  double sumFlowFromM2Node; // The amount of flow from the memory node in this physical node

  explicit PhysData(unsigned int physNodeIndex, double sumFlowFromM2Node = 0.0)
      : physNodeIndex(physNodeIndex), sumFlowFromM2Node(sumFlowFromM2Node) { }

  friend std::ostream& operator<<(std::ostream& out, const PhysData& data)
  {
    return out << "physNodeIndex: " << data.physNodeIndex << ", sumFlowFromM2Node: " << data.sumFlowFromM2Node;
  }
};

struct LayerTeleFlowData {
  unsigned int numNodes = 0;
  double teleportFlow = 0.0;
  double teleportWeight = 0.0;

  LayerTeleFlowData() = default;
  LayerTeleFlowData(double flow, double weight) : teleportFlow(flow), teleportWeight(weight) { }

  LayerTeleFlowData& operator+=(const LayerTeleFlowData& other)
  {
    numNodes += other.numNodes;
    teleportFlow += other.teleportFlow;
    teleportWeight += other.teleportWeight;
    return *this;
  }

  LayerTeleFlowData& operator-=(const LayerTeleFlowData& other)
  {
    numNodes -= other.numNodes;
    teleportFlow -= other.teleportFlow;
    teleportWeight -= other.teleportWeight;
    return *this;
  }

  // LayerTeleFlowData& operator=(const LayerTeleFlowData& other)
  // {
  //   numNodes = other.numNodes;
  //   teleportFlow = other.teleportFlow;
  //   teleportWeight = other.teleportWeight;
  //   return *this;
  // }

  bool isEmpty() const
  {
    return numNodes == 0;
  }

  friend std::ostream& operator<<(std::ostream& out, const LayerTeleFlowData& data)
  {
    return out << "{" << data.numNodes << "|" << data.teleportFlow << "|" << data.teleportWeight << "}";
  }
};

// For regularized multilayer flow
struct MultiFlowData : FlowData {
  std::map<unsigned int, LayerTeleFlowData> layerTeleFlowData; // layer -> LayerTeleFlowData

  MultiFlowData() = default;
  MultiFlowData(double flow) : FlowData(flow) { }
  MultiFlowData(const FlowData& data) : FlowData(data) { }
  virtual ~MultiFlowData() = default;

  MultiFlowData& operator+=(const MultiFlowData& other)
  {
    FlowData::operator+=(other);
    for (auto& it : other.layerTeleFlowData) {
      layerTeleFlowData[it.first] += it.second;
    }
    return *this;
  }

  MultiFlowData& operator-=(const MultiFlowData& other)
  {
    FlowData::operator-=(other);
    for (auto& it : other.layerTeleFlowData) {
      auto& data = layerTeleFlowData[it.first];
      data -= it.second;
      if (data.isEmpty()) {
        layerTeleFlowData.erase(it.first);
      }
    }
    return *this;
  }

  void init(const StateNetwork::StateNode& node) override
  {
    FlowData::init(node);
    layerTeleFlowData[node.layerId] = LayerTeleFlowData(node.intraLayerTeleFlow, node.intraLayerTeleWeight);
  }

  unsigned int numLayers() const { return layerTeleFlowData.size(); }

  friend std::ostream& operator<<(std::ostream& out, const MultiFlowData& data)
  {
    out << "[MultiFlow: ";
    for (auto& it : data.layerTeleFlowData) {
      out << "(" << it.first << ": " << it.second << "), ";
    }
    out << "]";
    return out;
  }
};

} // namespace infomap

#endif // FLOWDATA_H_
