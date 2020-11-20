/*
 * FlowData.h
 */

#ifndef SRC_CLUSTERING_FLOWDATA_H_
#define SRC_CLUSTERING_FLOWDATA_H_

#include <ostream>
#include <utility>
#include "DoublePair.h"

namespace infomap {

struct FlowData {
  using FlowType = std::pair<double, double>;

  FlowData(FlowType flow = {})
      : flow(flow),
        enterFlow({}),
        exitFlow({}) {}

  FlowData(const FlowData& other)
      : flow(other.flow),
        enterFlow(other.enterFlow),
        exitFlow(other.exitFlow) {}

  FlowData& operator=(const FlowData& other)
  {
    flow = other.flow;
    enterFlow = other.enterFlow;
    exitFlow = other.exitFlow;
    return *this;
  }

  FlowType flow{};
  FlowType enterFlow{};
  FlowType exitFlow{};

  FlowData& operator+=(const FlowData& other)
  {
    flow += other.flow;
    enterFlow += other.enterFlow;
    exitFlow += other.exitFlow;
    return *this;
  }

  FlowData& operator-=(const FlowData& other)
  {
    flow -= other.flow;
    enterFlow -= other.enterFlow;
    exitFlow -= other.exitFlow;
    return *this;
  }

  friend std::ostream& operator<<(std::ostream& out, const FlowData& data)
  {
    return out << "flow: " << data.flow << ", enter: " << data.enterFlow << ", exit: " << data.exitFlow;
  }
};


struct DeltaFlow {
  using FlowType = std::pair<double, double>;

  unsigned int module = 0;
  FlowType deltaExit{};
  FlowType deltaEnter{};
  unsigned int count = 0;

  virtual ~DeltaFlow() = default;

  DeltaFlow() = default;

  explicit DeltaFlow(unsigned int module, FlowType deltaExit, FlowType deltaEnter)
      : module(module),
        deltaExit(deltaExit),
        deltaEnter(deltaEnter),
        count(0) {}

  DeltaFlow(const DeltaFlow&) = default;
  DeltaFlow& operator=(const DeltaFlow&) = default;
  DeltaFlow(DeltaFlow&&) = default;
  DeltaFlow& operator=(DeltaFlow&&) = default;

  DeltaFlow& operator+=(const DeltaFlow& other)
  {
    module = other.module;
    deltaExit += other.deltaExit;
    deltaEnter += other.deltaEnter;
    ++count;
    return *this;
  }

  void reset()
  {
    module = 0;
    deltaExit = {};
    deltaEnter = {};
    count = 0;
  }

  friend void swap(DeltaFlow& first, DeltaFlow& second)
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
  using FlowType = std::pair<double, double>;

  FlowType sumDeltaPlogpPhysFlow{};
  FlowType sumPlogpPhysFlow{};

  MemDeltaFlow() : DeltaFlow() {}

  explicit MemDeltaFlow(unsigned int module, FlowType deltaExit, FlowType deltaEnter, FlowType sumDeltaPlogpPhysFlow = {}, FlowType sumPlogpPhysFlow = {})
      : DeltaFlow(module, deltaExit, deltaEnter),
        sumDeltaPlogpPhysFlow(sumDeltaPlogpPhysFlow),
        sumPlogpPhysFlow(sumPlogpPhysFlow) {}

  MemDeltaFlow& operator+=(const MemDeltaFlow& other)
  {
    DeltaFlow::operator+=(other);
    sumDeltaPlogpPhysFlow += other.sumDeltaPlogpPhysFlow;
    sumPlogpPhysFlow += other.sumPlogpPhysFlow;
    return *this;
  }

  void reset()
  {
    DeltaFlow::reset();
    sumDeltaPlogpPhysFlow = {};
    sumPlogpPhysFlow = {};
  }

  friend void swap(MemDeltaFlow& first, MemDeltaFlow& second)
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
  explicit PhysData(unsigned int physNodeIndex, double sumFlowFromM2Node = 0.0)
      : physNodeIndex(physNodeIndex), sumFlowFromM2Node(sumFlowFromM2Node) {}

  PhysData(const PhysData& other) = default;

  unsigned int physNodeIndex;
  double sumFlowFromM2Node; // The amount of flow from the memory node in this physical node

  friend std::ostream& operator<<(std::ostream& out, const PhysData& data)
  {
    return out << "physNodeIndex: " << data.physNodeIndex << ", sumFlowFromM2Node: " << data.sumFlowFromM2Node;
  }
};


} // namespace infomap


#endif /* SRC_CLUSTERING_FLOWDATA_H_ */
