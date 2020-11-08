/*
 * FlowData.h
 */

#ifndef _FLOWDATA_H_
#define _FLOWDATA_H_

#include <ostream>
#include <utility>

namespace infomap {

struct FlowData {
  using value_type = double;

  FlowData() = default;

  explicit FlowData(value_type flow) : flow(flow) {}

  value_type flow = 0.0;
  value_type enterFlow = 0.0;
  value_type exitFlow = 0.0;

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
  using value_type = double;

  unsigned int module = 0;
  value_type deltaExit = 0.0;
  value_type deltaEnter = 0.0;
  unsigned int count = 0;

  DeltaFlow() = default;

  DeltaFlow(unsigned int module, value_type deltaExit, value_type deltaEnter)
      : module(module), deltaExit(deltaExit), deltaEnter(deltaEnter) {}

  DeltaFlow(const DeltaFlow&) = default;
  DeltaFlow& operator=(const DeltaFlow&) = default;
  DeltaFlow(DeltaFlow&&) = default;
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
  using value_type = double;

  value_type sumDeltaPlogpPhysFlow{};
  value_type sumPlogpPhysFlow{};

  MemDeltaFlow() : DeltaFlow() {}

  MemDeltaFlow(unsigned int module, value_type deltaExit, value_type deltaEnter, value_type sumDeltaPlogpPhysFlow = 0.0, value_type sumPlogpPhysFlow = 0.0)
      : DeltaFlow(module, deltaExit, deltaEnter), sumDeltaPlogpPhysFlow(sumDeltaPlogpPhysFlow), sumPlogpPhysFlow(sumPlogpPhysFlow) {}

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
  using value_type = double;

  explicit PhysData(unsigned int physNodeIndex) : physNodeIndex(physNodeIndex) {}

  explicit PhysData(unsigned int physNodeIndex, value_type sumFlowFromM2Node)
      : physNodeIndex(physNodeIndex), sumFlowFromM2Node(sumFlowFromM2Node) {}

  unsigned int physNodeIndex;
  value_type sumFlowFromM2Node{}; // The amount of flow from the memory node in this physical node

  friend std::ostream& operator<<(std::ostream& out, const PhysData& data)
  {
    return out << "physNodeIndex: " << data.physNodeIndex << ", sumFlowFromM2Node: " << data.sumFlowFromM2Node;
  }
};


} // namespace infomap


#endif /* _FLOWDATA_H_ */
