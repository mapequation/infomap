#include "vendor/doctest.h"

#include "Infomap.h"
#include "io/InfomapError.h"

#include "TestUtils.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

using infomap::InfomapWrapper;

#ifdef _OPENMP
struct ScopedOmpThreadCount {
  explicit ScopedOmpThreadCount(int numThreads) : previousThreads(omp_get_max_threads())
  {
    omp_set_num_threads(numThreads);
  }

  ~ScopedOmpThreadCount()
  {
    omp_set_num_threads(previousThreads);
  }

  int previousThreads = 0;
};
#endif

struct FlowRunResult {
  std::map<unsigned int, unsigned int> modules;
  std::vector<std::vector<unsigned int>> partition;
  double codelength = 0.0;
  double indexCodelength = 0.0;
  unsigned int numTopModules = 0;
};

struct RegularizedMultilayerEgoFlow {
  double egoStateFlow = 0.0;
  double presentPeripheralStateFlow = 0.0;
  double absentPeripheralStateFlow = 0.0;
  double codelength = 0.0;
};

struct MultilayerIntraLink {
  unsigned int layer = 0;
  unsigned int source = 0;
  unsigned int target = 0;
  double weight = 0.0;
};

struct AnalyticStateFlow {
  unsigned int layer = 0;
  unsigned int physicalId = 0;
  double flow = 0.0;
};

struct AnalyticMultilayerFlow {
  std::vector<AnalyticStateFlow> stateFlows;
  std::map<unsigned int, double> physicalFlows;
  double codelength = 0.0;
};

double plogp(double value)
{
  return value <= 0.0 ? 0.0 : value * std::log2(value);
}

unsigned int findStateIndex(const std::vector<AnalyticStateFlow>& states, unsigned int layer, unsigned int physicalId)
{
  for (unsigned int i = 0; i < states.size(); ++i) {
    if (states[i].layer == layer && states[i].physicalId == physicalId) {
      return i;
    }
  }
  throw std::logic_error("Missing analytic multilayer state");
}

// Mirrors calcDirectedRegularizedMultilayerFlow for a network without self-link
// suppression. With skipAbsentNodes the prior spans only what each layer holds, the
// way --multilayer-skip-absent-nodes builds the state network; otherwise every layer
// holds every physical node. The two agree whenever the input fills the grid.
AnalyticMultilayerFlow analyticRegularizedMultilayerFlow(const std::vector<MultilayerIntraLink>& intraLinks, bool skipAbsentNodes = false, double intraStrength = 1.0, double interStrength = 1.0)
{
  std::map<unsigned int, unsigned int> layers;
  std::map<unsigned int, unsigned int> physicalIds;
  std::set<std::pair<unsigned int, unsigned int>> observed;
  for (const auto& link : intraLinks) {
    layers.emplace(link.layer, layers.size());
    physicalIds.emplace(link.source, physicalIds.size());
    physicalIds.emplace(link.target, physicalIds.size());
    observed.emplace(link.layer, link.source);
    observed.emplace(link.layer, link.target);
  }

  std::vector<AnalyticStateFlow> states;
  states.reserve(layers.size() * physicalIds.size());
  for (const auto& layer : layers) {
    for (const auto& physicalId : physicalIds) {
      if (!skipAbsentNodes || observed.count({ layer.first, physicalId.first }) > 0) {
        states.push_back({ layer.first, physicalId.first, 0.0 });
      }
    }
  }

  const auto numStates = states.size();
  const auto numLayers = layers.size();
  const auto numPhysicalIds = physicalIds.size();

  // How many nodes a layer holds, and how many layers hold a node.
  std::map<unsigned int, unsigned int> numNodesInLayer;
  std::map<unsigned int, unsigned int> numLayersOfPhysNode;
  for (const auto& state : states) {
    ++numNodesInLayer[state.layer];
    ++numLayersOfPhysNode[state.physicalId];
  }

  std::vector<double> sOut(numStates, 0.0);
  std::vector<double> sIn(numStates, 0.0);
  std::vector<unsigned int> kOut(numStates, 0);
  std::vector<unsigned int> kIn(numStates, 0);
  std::vector<std::vector<std::pair<unsigned int, double>>> intraProb(numStates);
  for (const auto& link : intraLinks) {
    const auto source = findStateIndex(states, link.layer, link.source);
    const auto target = findStateIndex(states, link.layer, link.target);
    sOut[source] += link.weight;
    ++kOut[source];
    sIn[target] += link.weight;
    ++kIn[target];
    intraProb[source].push_back({ target, link.weight });
  }

  // Continuous configuration model, per layer: c_ij = (sum_kappa/sum_sigma) u_out(i) u_in(j).
  std::map<unsigned int, double> minUOut;
  std::map<unsigned int, double> minUIn;
  for (unsigned int i = 0; i < numStates; ++i) {
    const auto layer = states[i].layer;
    if (kOut[i] > 0) {
      const double u = sOut[i] / kOut[i];
      auto it = minUOut.find(layer);
      minUOut[layer] = it == minUOut.end() ? u : std::min(it->second, u);
    }
    if (kIn[i] > 0) {
      const double u = sIn[i] / kIn[i];
      auto it = minUIn.find(layer);
      minUIn[layer] = it == minUIn.end() ? u : std::min(it->second, u);
    }
  }

  std::vector<double> uOut(numStates, 0.0);
  std::vector<double> uIn(numStates, 0.0);
  std::map<unsigned int, double> sumUIn;
  std::map<unsigned int, double> sumDegree;
  std::map<unsigned int, double> sumStrength;
  std::map<unsigned int, double> sumSOut;
  for (unsigned int i = 0; i < numStates; ++i) {
    const auto layer = states[i].layer;
    uOut[i] = kOut[i] == 0 ? (minUOut.count(layer) ? minUOut.at(layer) : 1.0) : sOut[i] / kOut[i];
    uIn[i] = kIn[i] == 0 ? (minUIn.count(layer) ? minUIn.at(layer) : 1.0) : sIn[i] / kIn[i];
    sumUIn[layer] += uIn[i];
    sumDegree[layer] += kIn[i] + kOut[i];
    sumStrength[layer] += sIn[i] + sOut[i];
    sumSOut[layer] += sOut[i];
  }

  std::vector<double> intraPriorOutWeight(numStates, 0.0);
  std::vector<double> teleportWeight(numStates, 0.0);
  std::vector<double> interOutWeight(numStates, 0.0);
  std::vector<std::vector<unsigned int>> interTargets(numStates);
  for (unsigned int i = 0; i < numStates; ++i) {
    const auto layer = states[i].layer;
    const auto numLayersForNode = numLayersOfPhysNode.at(states[i].physicalId);
    const double lambdaIntra = intraStrength * std::log(static_cast<double>(numPhysicalIds)) / (static_cast<double>(numLayersForNode) * numPhysicalIds);
    const double invMeanWeight = sumStrength.at(layer) > 0 ? sumDegree.at(layer) / sumStrength.at(layer) : 1.0;

    intraPriorOutWeight[i] = lambdaIntra * invMeanWeight * uOut[i] * sumUIn.at(layer);
    if (intraStrength == 0.0) {
      // No prior to spread: teleportation falls back to the unregularized model and
      // follows the layer's out-link weights.
      teleportWeight[i] = sumSOut.at(layer) > 0 ? sOut[i] / sumSOut.at(layer) : 1.0 / numNodesInLayer.at(layer);
    } else {
      teleportWeight[i] = sumUIn.at(layer) > 0 ? uIn[i] / sumUIn.at(layer) : 1.0 / numNodesInLayer.at(layer);
    }
    // A node in a single layer has no inter-layer prior: ln(1) = 0.
    interOutWeight[i] = numLayersForNode < 2 ? 0.0 : interStrength * std::log(static_cast<double>(numLayersForNode));
    for (unsigned int j = 0; j < numStates; ++j) {
      if (states[j].physicalId == states[i].physicalId) {
        interTargets[i].push_back(j);
      }
    }
  }
  for (unsigned int source = 0; source < numStates; ++source) {
    if (sOut[source] == 0.0) {
      continue;
    }
    for (auto& link : intraProb[source]) {
      link.second /= sOut[source];
    }
  }

  std::vector<double> alpha(numStates, 0.0);
  std::vector<double> alphaInter(numStates, 0.0);
  for (unsigned int i = 0; i < numStates; ++i) {
    alpha[i] = sOut[i] == 0.0 ? 1.0 : intraPriorOutWeight[i] / (intraPriorOutWeight[i] + sOut[i]);
    double intraOutTotal = sOut[i] + intraPriorOutWeight[i];
    if (intraStrength == 0.0 && kOut[i] == 0) {
      // The unrecorded teleportation replaces the missing out-links, so it carries the
      // out-strength of a typical node of the layer against the inter-layer step.
      const auto layer = states[i].layer;
      intraOutTotal = sumSOut.at(layer) / numNodesInLayer.at(layer);
    }
    const double sumOut = interOutWeight[i] + intraOutTotal;
    alphaInter[i] = sumOut == 0.0 ? 0.0 : interOutWeight[i] / sumOut;
  }

  std::vector<double> stateFlow(numStates, 1.0 / static_cast<double>(numStates));
  std::vector<double> nextFlow(numStates, 0.0);
  std::vector<double> unrecordedInterFlow(numStates, 0.0);
  std::vector<double> layerTeleFlow(numLayers, 0.0);

  for (unsigned int iteration = 0; iteration < 1000; ++iteration) {
    std::fill(nextFlow.begin(), nextFlow.end(), 0.0);
    std::fill(unrecordedInterFlow.begin(), unrecordedInterFlow.end(), 0.0);
    std::fill(layerTeleFlow.begin(), layerTeleFlow.end(), 0.0);

    for (unsigned int source = 0; source < numStates; ++source) {
      const auto& targets = interTargets[source];
      if (interOutWeight[source] == 0.0) {
        continue;
      }
      for (const auto target : targets) {
        unrecordedInterFlow[target] += alphaInter[source] * stateFlow[source] / static_cast<double>(targets.size());
      }
    }

    for (unsigned int i = 0; i < numStates; ++i) {
      const auto layerIndex = layers.at(states[i].layer);
      layerTeleFlow[layerIndex] += alpha[i] * ((1.0 - alphaInter[i]) * stateFlow[i] + unrecordedInterFlow[i]);
    }

    for (unsigned int i = 0; i < numStates; ++i) {
      const auto layerIndex = layers.at(states[i].layer);
      nextFlow[i] += layerTeleFlow[layerIndex] * teleportWeight[i];
    }

    for (unsigned int source = 0; source < numStates; ++source) {
      const double sourceFlow = (1.0 - alphaInter[source]) * stateFlow[source] + unrecordedInterFlow[source];
      for (const auto& link : intraProb[source]) {
        nextFlow[link.first] += (1.0 - alpha[source]) * link.second * sourceFlow;
      }
    }

    double error = 0.0;
    double sumFlow = 0.0;
    for (unsigned int i = 0; i < numStates; ++i) {
      error += std::abs(nextFlow[i] - stateFlow[i]);
      sumFlow += nextFlow[i];
    }
    for (auto& flow : nextFlow) {
      flow /= sumFlow;
    }
    stateFlow = nextFlow;
    if (error < 1e-15 && iteration >= 50) {
      break;
    }
  }

  if (intraStrength == 0.0) {
    // Teleportation is unrecorded here, so a visit is an intra-layer link step: take one
    // last step without teleportation and renormalize to the flow that moves along links.
    std::vector<double> interStep(numStates, 0.0);
    for (unsigned int source = 0; source < numStates; ++source) {
      const auto& targets = interTargets[source];
      if (interOutWeight[source] == 0.0) {
        continue;
      }
      for (const auto target : targets) {
        interStep[target] += alphaInter[source] * stateFlow[source] / static_cast<double>(targets.size());
      }
    }

    std::vector<double> intraStepFlow(numStates, 0.0);
    double movingFlow = 0.0;
    for (unsigned int i = 0; i < numStates; ++i) {
      intraStepFlow[i] = (1.0 - alphaInter[i]) * stateFlow[i] + interStep[i];
      if (kOut[i] > 0) {
        movingFlow += intraStepFlow[i];
      }
    }

    if (movingFlow > 0.0) {
      std::fill(stateFlow.begin(), stateFlow.end(), 0.0);
      for (unsigned int source = 0; source < numStates; ++source) {
        for (const auto& link : intraProb[source]) {
          stateFlow[link.first] += (1.0 - alpha[source]) * link.second * intraStepFlow[source] / movingFlow;
        }
      }
    }
  }

  AnalyticMultilayerFlow result;
  result.stateFlows = states;
  for (unsigned int i = 0; i < numStates; ++i) {
    result.stateFlows[i].flow = stateFlow[i];
    result.physicalFlows[states[i].physicalId] += stateFlow[i];
  }
  for (const auto& physicalFlow : result.physicalFlows) {
    result.codelength -= plogp(physicalFlow.second);
  }
  return result;
}

void addMultilayerIntraLinks(InfomapWrapper& im, const std::vector<MultilayerIntraLink>& intraLinks)
{
  for (const auto& link : intraLinks) {
    im.addMultilayerIntraLink(link.layer, link.source, link.target, link.weight);
  }
}

void checkRegularizedMultilayerFlow(InfomapWrapper& im, const AnalyticMultilayerFlow& expected)
{
  infomap::test::checkRunSanity(im);
  infomap::test::checkApproxCodelength(im.codelength(), expected.codelength, 1e-9);

  std::map<std::pair<unsigned int, unsigned int>, double> expectedStateFlows;
  for (const auto& stateFlow : expected.stateFlows) {
    expectedStateFlows[{ stateFlow.layer, stateFlow.physicalId }] = stateFlow.flow;
  }

  unsigned int numStates = 0;
  for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
    const auto& node = *it;
    const auto expectedFlow = expectedStateFlows.at({ node.layerId, node.physicalId });
    CHECK(node.data.flow == doctest::Approx(expectedFlow).epsilon(1e-10));
    ++numStates;
  }
  // Catches a state node the model should not have built, which an .at() lookup over
  // the expected set cannot see.
  CHECK(numStates == expectedStateFlows.size());
}

std::array<double, 3> solve3x3(std::array<std::array<double, 3>, 3> coefficients, std::array<double, 3> rhs)
{
  for (unsigned int pivot = 0; pivot < 3; ++pivot) {
    const auto pivotValue = coefficients[pivot][pivot];
    for (unsigned int col = pivot; col < 3; ++col) {
      coefficients[pivot][col] /= pivotValue;
    }
    rhs[pivot] /= pivotValue;

    for (unsigned int row = 0; row < 3; ++row) {
      if (row == pivot) {
        continue;
      }
      const auto factor = coefficients[row][pivot];
      for (unsigned int col = pivot; col < 3; ++col) {
        coefficients[row][col] -= factor * coefficients[pivot][col];
      }
      rhs[row] -= factor * rhs[pivot];
    }
  }

  return rhs;
}

RegularizedMultilayerEgoFlow analyticRegularizedMultilayerEgoFlow()
{
  constexpr double numPhysicalNodes = 5.0;
  constexpr double numLayers = 2.0;
  constexpr double intraOutWeightPerState = 2.0;

  // Current implementation of the manuscript prior strengths:
  // gamma_intra out-weight = log(N_phys) / L, and each state has L vertical
  // inter-layer prior links with weight log(L) / L, for total log(L).
  const double intraPriorOutWeight = std::log(numPhysicalNodes) / numLayers;
  const double interPriorOutWeight = std::log(numLayers);

  const double alpha = intraPriorOutWeight / (intraPriorOutWeight + intraOutWeightPerState);
  const double beta = 1.0 - alpha;
  const double alphaInterPresent = interPriorOutWeight / (interPriorOutWeight + intraOutWeightPerState + intraPriorOutWeight);
  const double alphaInterAbsent = interPriorOutWeight / (interPriorOutWeight + intraPriorOutWeight);

  // Symmetry reduces the ten state-node flows to three variables:
  // x: ego state present in both layers,
  // y: peripheral state present in its triangle layer,
  // z: peripheral state absent from the opposite layer but created by vertical priors.
  //
  // The two-step model first moves unrecorded vertical flow to counterparts, then
  // redistributes recorded intralayer prior flow within each layer.
  const double presentBaseFromY = 1.0 - alphaInterPresent / 2.0;
  const double presentBaseFromZ = alphaInterAbsent / 2.0;
  const double absentBaseFromY = alphaInterPresent / 2.0;
  const double absentBaseFromZ = 1.0 - alphaInterAbsent / 2.0;

  const std::array<double, 3> layerTeleportFlowCoefficients = {
      alpha,
      2.0 * alpha * presentBaseFromY + 2.0 * absentBaseFromY,
      2.0 * alpha * presentBaseFromZ + 2.0 * absentBaseFromZ,
  };
  const std::array<double, 3> presentBaseCoefficients = { 0.0, presentBaseFromY, presentBaseFromZ };

  std::array<std::array<double, 3>, 3> coefficients = {{
      {{
          1.0 - layerTeleportFlowCoefficients[0] / numPhysicalNodes - beta * presentBaseCoefficients[0],
          -layerTeleportFlowCoefficients[1] / numPhysicalNodes - beta * presentBaseCoefficients[1],
          -layerTeleportFlowCoefficients[2] / numPhysicalNodes - beta * presentBaseCoefficients[2],
      }},
      {{
          -layerTeleportFlowCoefficients[0] / numPhysicalNodes - beta / 2.0,
          1.0 - layerTeleportFlowCoefficients[1] / numPhysicalNodes - beta * presentBaseCoefficients[1] / 2.0,
          -layerTeleportFlowCoefficients[2] / numPhysicalNodes - beta * presentBaseCoefficients[2] / 2.0,
      }},
      {{ 2.0, 4.0, 4.0 }},
  }};
  const auto solution = solve3x3(coefficients, {{ 0.0, 0.0, 1.0 }});

  const double egoPhysicalFlow = 2.0 * solution[0];
  const double peripheralPhysicalFlow = solution[1] + solution[2];
  const double codelength = -plogp(egoPhysicalFlow) - 4.0 * plogp(peripheralPhysicalFlow);

  return { solution[0], solution[1], solution[2], codelength };
}

FlowRunResult runDirectedFixture(const std::string& extraFlags)
{
  InfomapWrapper im(infomap::test::defaultFlags("--directed " + extraFlags));
  infomap::test::readNetworkFixture(im, "teleport_directed.net");

  im.run();

  auto modules = im.getModules();
  infomap::test::checkRunSanity(im);
  return {
      modules,
      infomap::test::canonicalPartition(modules),
      im.codelength(),
      im.getIndexCodelength(),
      im.numTopModules(),
  };
}

void checkInnerParallelPartitionCodelength(const std::string& extraFlags, const std::string& clusterPath)
{
  const auto result = runDirectedFixture("--inner-parallelization " + extraFlags);
  {
    std::ofstream out(clusterPath.c_str());
    out << "# node_id module\n";
    for (const auto& module : result.modules) {
      out << module.first << " " << module.second << "\n";
    }
  }

  InfomapWrapper check(infomap::test::defaultFlags("--directed --no-infomap --cluster-data " + clusterPath + " " + extraFlags));
  infomap::test::readNetworkFixture(check, "teleport_directed.net");
  check.run();
  std::remove(clusterPath.c_str());

  infomap::test::checkApproxCodelength(check.codelength(), result.codelength, 1e-9);
  infomap::test::checkApproxCodelength(check.getIndexCodelength(), result.indexCodelength, 1e-9);
}

// --inner-parallelization only engages on an active network of at least
// minNetworkSizeForInnerParallelization (10,000) nodes, so the fixtures above take
// the serial path. This directed planted partition is exactly that large: groups
// of 40 nodes, each node with four random draws inside its group (self-links
// skipped, repeats kept) and one link to a random node anywhere.
void addInnerParallelThresholdNetwork(InfomapWrapper& im)
{
  const unsigned int groupSize = 40;
  const unsigned int numNodes = 10000;
  std::mt19937 rng(17);
  for (unsigned int source = 0; source < numNodes; ++source) {
    const unsigned int groupStart = source - source % groupSize;
    for (unsigned int k = 0; k < 4; ++k) {
      unsigned int target = groupStart + rng() % groupSize;
      if (target != source)
        im.addLink(source, target);
    }
    im.addLink(source, rng() % numNodes);
  }
}

struct InnerParallelThresholdRun {
  std::map<unsigned int, unsigned int> modules;
  double codelength = 0.0;
  double indexCodelength = 0.0;
  unsigned long threadsUsed = 0;
};

InnerParallelThresholdRun runInnerParallelThresholdNetwork(const std::string& flags)
{
  const std::string reportPath = "inner_parallel_threshold_report.json";
  std::remove(reportPath.c_str());
  InfomapWrapper im(infomap::test::defaultFlags("--directed --two-level --no-file-output --timing-json " + reportPath + " " + flags));
  addInnerParallelThresholdNetwork(im);
  im.run();
  infomap::test::checkRunSanity(im);

  const auto report = nlohmann::json::parse(infomap::test::readTextFile(reportPath));
  std::remove(reportPath.c_str());
  return { im.getModules(), im.codelength(), im.getIndexCodelength(), report.at("threads_used").get<unsigned long>() };
}

TEST_CASE("Recorded teleportation stays stable across trial counts for the directed fixture [fast][core][flow][columnar-contract]")
{
  const auto oneTrial = runDirectedFixture("--recorded-teleportation --num-trials 1");
  const auto twoTrials = runDirectedFixture("--recorded-teleportation --num-trials 2");

  CHECK(oneTrial.partition == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
  CHECK(oneTrial.partition == twoTrials.partition);
  CHECK(oneTrial.codelength == doctest::Approx(twoTrials.codelength));
  CHECK(oneTrial.indexCodelength == doctest::Approx(twoTrials.indexCodelength));
}

TEST_CASE("Directed to-nodes teleportation keeps the expected coarse partition [fast][core][flow][columnar-contract]")
{
  const auto result = runDirectedFixture("--to-nodes");

  CHECK(result.numTopModules == 2);
  CHECK(result.partition == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
}

TEST_CASE("Node weights set through addNode reach the flow like weights from a file [fast][core][flow]")
{
  // postProcessInputData used to reset addNode(id, weight) to the physical node's
  // default weight 1, so --to-nodes and --use-node-weights-as-flow saw uniform
  // weights for a network built through the API (#1183). Node 9 has no links.
  const std::string path = "api_node_weights.net";
  {
    std::ofstream out(path.c_str());
    out << "*Vertices 4\n1 \"1\"\n2 \"2\"\n3 \"3\"\n9 \"9\" 3\n"
        << "*Arcs\n1 2 1.5\n2 1 2\n2 3 1\n3 3 1\n";
  }
  auto addLinks = [](InfomapWrapper& im) {
    im.addLink(1, 2, 1.5);
    im.addLink(2, 1, 2.0);
    im.addLink(2, 3, 1.0);
    im.addLink(3, 3, 1.0);
  };
  auto leafFlows = [](InfomapWrapper& im) {
    std::map<unsigned int, double> flows;
    for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
      flows[it->physicalId] = it->data.flow;
    }
    return flows;
  };

  for (const std::string flags : { std::string("--directed --to-nodes --recorded-teleportation"),
                                   std::string("--directed --use-node-weights-as-flow") }) {
    CAPTURE(flags);
    InfomapWrapper fromFile(infomap::test::defaultFlags(flags));
    fromFile.readInputData(path);
    fromFile.run();

    InfomapWrapper fromApi(infomap::test::defaultFlags(flags));
    fromApi.addNode(9, 3.0);
    addLinks(fromApi);
    fromApi.run();

    const auto expected = leafFlows(fromFile);
    const auto actual = leafFlows(fromApi);
    REQUIRE(actual.size() == expected.size());
    for (const auto& entry : expected) {
      CHECK(actual.at(entry.first) == doctest::Approx(entry.second).epsilon(1e-12));
    }
    CHECK(fromApi.codelength() == doctest::Approx(fromFile.codelength()).epsilon(1e-12));
  }

  // --use-node-weights-as-flow gives node 9 its share of the weight directly.
  InfomapWrapper asFlow(infomap::test::defaultFlags("--directed --use-node-weights-as-flow"));
  asFlow.addNode(9, 3.0);
  addLinks(asFlow);
  asFlow.run();
  CHECK(leafFlows(asFlow).at(9) == doctest::Approx(0.5));

  // A weight read from a file after addNode replaces it: the last one given wins.
  InfomapWrapper overridden(infomap::test::defaultFlags("--directed --use-node-weights-as-flow"));
  overridden.addNode(9, 7.0);
  overridden.readInputData(path, true);
  overridden.run();
  CHECK(leafFlows(overridden).at(9) == doctest::Approx(0.5));

  std::remove(path.c_str());
}

TEST_CASE("Directed regularization remains numerically sane on the teleportation fixture [fast][core][flow]")
{
  const auto result = runDirectedFixture("--regularized");

  CHECK(result.numTopModules == 1);
  CHECK(result.partition == std::vector<std::vector<unsigned int>>{{1, 2, 3, 4, 5, 6}});
  CHECK(result.codelength > result.indexCodelength);
  infomap::test::checkApproxCodelength(result.codelength, 2.575842872);
}

TEST_CASE("Undirected regularization remains stable on the two-triangles fixture [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--regularized"));
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 1);
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3, 4, 5, 6}});
  infomap::test::checkApproxCodelength(im.codelength(), 2.575767408, 1e-9);
}

TEST_CASE("Directed regularization on both directions of a link with a self-link is the undirected one [fast][core][flow]")
{
  // The directed prior's average link weight counted a self-link once, as an undirected
  // link end, while its out- and in-weights count it as both an out-link and an in-link (#1204).
  const auto run = [](bool directed) {
    InfomapWrapper im(infomap::test::defaultFlags(std::string("--regularized --two-level -N 1 --seed 1") + (directed ? " --directed" : "")));
    for (const auto& [source, target] : std::vector<std::pair<unsigned int, unsigned int>> { { 1, 2 }, { 1, 3 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 4, 6 }, { 5, 6 } }) {
      im.addLink(source, target);
      if (directed) im.addLink(target, source);
    }
    im.addLink(5, 5, 2.0);
    im.run();
    std::map<unsigned int, double> flow;
    for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
      flow[it->physicalId] = it->data.flow;
    }
    return std::make_pair(im.codelength(), flow);
  };

  const auto undirected = run(false);
  const auto directed = run(true);
  CHECK(undirected.second.at(5) == doctest::Approx(0.233721).epsilon(1e-5));
  for (const auto& entry : undirected.second) {
    CAPTURE(entry.first);
    CHECK(directed.second.at(entry.first) == doctest::Approx(entry.second).epsilon(1e-12));
  }
  infomap::test::checkApproxCodelength(directed.first, undirected.first, 1e-12);
}

namespace {

// Two triangles joined by the link 3 4. With split, the links 1 2 and 3 4 are each listed as
// half the weight in both directions, the second one reverse first.
void addPairTriangles(InfomapWrapper& im, bool split)
{
  for (const auto& [source, target] : std::vector<std::pair<unsigned int, unsigned int>> { { 1, 3 }, { 2, 3 }, { 4, 5 }, { 4, 6 }, { 5, 6 } }) {
    im.addLink(source, target, 1.0);
  }
  if (split) {
    im.addLink(1, 2, 0.5);
    im.addLink(2, 1, 0.5);
    im.addLink(4, 3, 0.5);
    im.addLink(3, 4, 0.5);
  } else {
    im.addLink(1, 2, 1.0);
    im.addLink(3, 4, 1.0);
  }
}

} // namespace

TEST_CASE("A link a b and its opposite link b a are one link of an undirected run [fast][core][flow]")
{
  // Duplicate links are summed on input only when they have the same direction. An undirected
  // run sums opposite links itself, so every degree it uses counts them once (#1366).
  for (const std::string flags : { "", "--entropy-corrected", "--regularized", "--variable-markov-time" }) {
    CAPTURE(flags);
    InfomapWrapper single(infomap::test::defaultFlags(flags));
    addPairTriangles(single, false);
    single.run();
    InfomapWrapper split(infomap::test::defaultFlags(flags));
    addPairTriangles(split, true);
    split.run();

    CHECK(split.network().numLinks() == 7);
    CHECK(split.network().numLinks() == single.network().numLinks());
    CHECK(split.codelength() == single.codelength());
    CHECK(split.getModules() == single.getModules());
  }
}

TEST_CASE("Opposite links a b and b a stay two links for the directed codings [fast][core][flow]")
{
  for (const std::string flags : { "--directed", "--flow-model undirdir" }) {
    CAPTURE(flags);
    InfomapWrapper im(infomap::test::defaultFlags(flags));
    addPairTriangles(im, true);
    im.run();
    CHECK(im.network().numLinks() == 9);
  }
}

TEST_CASE("A directed run after an undirected one reads the links as entered [fast][core][flow]")
{
  // The bindings choose the flow model per run, after the links are added, so the undirected
  // run must not sum opposite links in the stored links (#1366).
  InfomapWrapper fresh(infomap::test::defaultFlags("--directed"));
  addPairTriangles(fresh, true);
  fresh.run();

  InfomapWrapper im(infomap::test::defaultFlags());
  addPairTriangles(im, true);
  im.run();
  CHECK(im.network().numLinks() == 7);
  im.run("--directed");
  CHECK(im.network().numLinks() == 9);
  CHECK(im.codelength() == fresh.codelength());
  CHECK(im.getModules() == fresh.getModules());
  im.run();
  CHECK(im.network().numLinks() == 7);
}

TEST_CASE("A directed run after an undirected one gets uneven opposite weights back exactly [fast][core][flow]")
{
  // The undirected view sums 0.1 into 0.2, and 0.3000000000000000444 - 0.2 is not 0.1, so the
  // links as entered must come back from what was set aside, not by subtracting (#1366). The
  // dropped links 5 1, 6 3 and 7 4 sit at the start, middle and end of their rows.
  const auto addLinks = [](InfomapWrapper& im) {
    im.addLink(1, 5, 0.1);
    im.addLink(5, 1, 0.2);
    im.addLink(5, 6, 1.0);
    im.addLink(3, 6, 0.1);
    im.addLink(6, 2, 1.0);
    im.addLink(6, 3, 0.2);
    im.addLink(6, 7, 1.0);
    im.addLink(4, 7, 0.1);
    im.addLink(7, 2, 1.0);
    im.addLink(7, 4, 0.7);
  };
  using Link = std::tuple<unsigned int, unsigned int, double>;
  const auto linksOf = [](InfomapWrapper& im) {
    std::vector<Link> links;
    im.network().forEachLink([&](unsigned int s, unsigned int t, double weight, double&) { links.emplace_back(s, t, weight); });
    return links;
  };

  InfomapWrapper fresh(infomap::test::defaultFlags("--directed"));
  addLinks(fresh);
  fresh.run();

  InfomapWrapper im(infomap::test::defaultFlags());
  addLinks(im);
  im.run();
  CHECK(im.network().numLinks() == 7);
  // Node indices follow the sorted node ids, so index 0 is node 1 and index 4 is node 5
  const auto view = linksOf(im);
  CHECK(std::find(view.begin(), view.end(), Link { 0, 4, 0.1 + 0.2 }) != view.end());
  CHECK(std::find(view.begin(), view.end(), Link { 3, 6, 0.1 + 0.7 }) != view.end());

  im.run("--directed");
  CHECK(im.network().numLinks() == 10);
  CHECK(linksOf(im) == linksOf(fresh));
  CHECK(im.codelength() == fresh.codelength());
  CHECK(im.getModules() == fresh.getModules());
}

TEST_CASE("Regularization strength 0 matches its small-strength limit with dangling nodes [fast][core][flow]")
{
  // At strength 0 the prior out-weight t_i is zero, so alpha = t_i / (s_i + t_i) was
  // 0/0 for a node without out-links. Directed flow went NaN and the run was rejected
  // as degenerate; undirected flow left a NaN teleport flow on the isolated node, which
  // blocked moves and returned a worse partition with a successful exit (#1144).
  // Strength 0 is the limit of small strengths, so it has to agree with one.
  const auto run = [](const std::string& flags, const std::string& strength) {
    InfomapWrapper im(infomap::test::defaultFlags("--regularized --regularization-strength " + strength + " " + flags));
    im.addLink(1, 2);
    im.addLink(2, 3);
    im.addLink(3, 1);
    im.addLink(4, 5);
    im.addLink(5, 6);
    im.addLink(6, 4);
    im.addLink(3, 4);
    im.addLink(6, 7); // 7 has no out-links in directed flow
    im.addNode(8); // isolated
    im.run();
    auto modules = im.getModules(1);
    // Undirected flow gives 8 no flow at strength 0, so every module fits it equally well.
    modules.erase(8);
    return std::make_pair(im.codelength(), infomap::test::canonicalPartition(modules));
  };

  for (const std::string flags : { "--directed", "--directed --no-self-links", "", "--no-self-links" }) {
    CAPTURE(flags);
    const auto [codelengthAtZero, partitionAtZero] = run(flags, "0");
    const auto [codelengthNearZero, partitionNearZero] = run(flags, "1e-12");

    CHECK(std::isfinite(codelengthAtZero));
    infomap::test::checkApproxCodelength(codelengthAtZero, codelengthNearZero, 1e-9);
    CHECK(partitionAtZero == partitionNearZero);
  }
}

TEST_CASE("Multilayer skip-absent-nodes needs a prior to restrict [fast][core][flow]")
{
  CHECK_THROWS_WITH_AS(
      InfomapWrapper(infomap::test::defaultFlags("--multilayer-skip-absent-nodes --two-level")),
      "--multilayer-skip-absent-nodes requires --regularized",
      std::runtime_error);
}

#if INFOMAP_FEATURE_REGULARIZED_MULTILAYER
// examples/networks/multilayer_intra_inter.net -- a network we ship -- used to fail
// its flow-imbalance check under --regularized (#1019). Its layers hold 3 of the 5
// physical nodes each, while the flow model spread teleportation over all 5, so each
// layer kept only 3/5 of the teleport flow it handed out and the rest vanished. The
// model now reads the per-layer node count off the state network it is given, which
// is what an *Intra/*Inter input builds: nodes only where the data puts them.
//
// The throw itself has to stay reachable as an error the caller can handle:
// calcDirectedRegularizedMultilayerFlow was noexcept, so a throw called
// std::terminate and the process died on SIGABRT with a raw libc++abi line instead
// of Infomap's own message. If the noexcept comes back, a future imbalance aborts
// the whole test binary rather than failing an assertion.
TEST_CASE("Regularized multilayer flow balances over each layer's own nodes [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--regularized --two-level"));
  im.readInputData(infomap::test::repoPath("examples/networks/multilayer_intra_inter.net"));

  CHECK_NOTHROW(im.run());
  infomap::test::checkRunSanity(im);

  // Only the 6 (node, layer) pairs the input mentions, and i is the one node in both
  // layers. Flow summing to 1 is what the imbalance check would have caught.
  double sumFlow = 0.0;
  unsigned int numStates = 0;
  for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
    sumFlow += (*it).data.flow;
    ++numStates;
  }
  CHECK(numStates == 6);
  CHECK(sumFlow == doctest::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("Regularized multilayer flow without self-links sums to 1 and balances each state (#1363) [fast][core][flow]")
{
  // --no-self-links inflates a state's teleportation rate to alpha / (1 - w) and leaves out
  // the part that lands on the state itself. The power iteration left it out of the state's
  // flow instead of its intra-step flow, so on multilayer_intra.net the total drifted from 1
  // and the run stopped. The link flows written after it followed the links at 1 - alpha with
  // the inflated rate, short of the walk's 1 - alpha (1 - w), so on multilayer_intra_inter.net
  // a state's enter and exit flow differed by up to 0.01.
  for (const std::string network : { "multilayer_intra.net", "multilayer_intra_inter.net" }) {
    for (const std::string flags : { "--regularized --no-self-links", "--regularized --no-self-links --directed" }) {
      CAPTURE(network);
      CAPTURE(flags);
      InfomapWrapper im(infomap::test::defaultFlags(flags + " --no-infomap --two-level"));
      im.readInputData(infomap::test::repoPath("examples/networks/" + network));
      REQUIRE_NOTHROW(im.run());

      double sumFlow = 0.0;
      double maxImbalance = 0.0;
      for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
        sumFlow += (*it).data.flow;
        maxImbalance = std::max(maxImbalance, std::abs((*it).data.enterFlow - (*it).data.exitFlow));
      }
      CHECK(sumFlow == doctest::Approx(1.0).epsilon(1e-12));
      CHECK(maxImbalance < 1e-12);
    }
  }
}

TEST_CASE("Regularized multilayer flow supports non-dense matchable state ids [fast][core][flow][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags(
      "--directed --regularized --matchable-multilayer-ids 2 --no-infomap --two-level"));
  im.addMultilayerIntraLink(1, 10, 20, 1.0);
  im.addMultilayerIntraLink(2, 10, 20, 1.0);
  im.addMultilayerInterLink(1, 10, 2, 1.0);
  im.addMultilayerInterLink(2, 10, 1, 1.0);

  CHECK_NOTHROW(im.run());
  infomap::test::checkRunSanity(im);
}

TEST_CASE("Regularized multilayer ego flow matches analytic prior-strength sample [fast][core][flow][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--directed --regularized --no-infomap --two-level"));

  auto addDirectedTriangle = [&im](unsigned int layer, unsigned int ego, unsigned int first, unsigned int second) {
    im.addMultilayerIntraLink(layer, ego, first, 1.0);
    im.addMultilayerIntraLink(layer, first, ego, 1.0);
    im.addMultilayerIntraLink(layer, ego, second, 1.0);
    im.addMultilayerIntraLink(layer, second, ego, 1.0);
    im.addMultilayerIntraLink(layer, first, second, 1.0);
    im.addMultilayerIntraLink(layer, second, first, 1.0);
  };
  addDirectedTriangle(1, 1, 2, 3);
  addDirectedTriangle(2, 1, 4, 5);

  im.run();

  const auto expected = analyticRegularizedMultilayerEgoFlow();
  infomap::test::checkRunSanity(im);
  infomap::test::checkApproxCodelength(im.codelength(), expected.codelength);

  for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
    const auto& node = *it;
    const bool presentInLayer = node.physicalId == 1
        || (node.layerId == 1 && (node.physicalId == 2 || node.physicalId == 3))
        || (node.layerId == 2 && (node.physicalId == 4 || node.physicalId == 5));
    const double expectedFlow = node.physicalId == 1
        ? expected.egoStateFlow
        : presentInLayer ? expected.presentPeripheralStateFlow
                         : expected.absentPeripheralStateFlow;

    CHECK(node.data.flow == doctest::Approx(expectedFlow).epsilon(1e-10));
  }
}

TEST_CASE("Regularized multilayer asymmetric flow matches analytic prior-strength sample [fast][core][flow][columnar-contract]")
{
  const std::vector<MultilayerIntraLink> intraLinks = {
      { 1, 1, 2, 2.0 },
      { 1, 2, 1, 1.0 },
      { 1, 1, 3, 1.0 },
      { 1, 3, 1, 1.0 },
      { 2, 1, 2, 1.0 },
      { 2, 2, 3, 3.0 },
      { 2, 3, 1, 1.0 },
  };

  InfomapWrapper im(infomap::test::defaultFlags("--directed --regularized --no-infomap --two-level"));
  addMultilayerIntraLinks(im, intraLinks);

  im.run();

  checkRegularizedMultilayerFlow(im, analyticRegularizedMultilayerFlow(intraLinks));
}

TEST_CASE("Regularized multilayer matchable state-id flow matches analytic prior-strength sample [fast][core][flow][columnar-contract]")
{
  const std::vector<MultilayerIntraLink> intraLinks = {
      { 1, 10, 20, 1.0 },
      { 1, 20, 10, 2.0 },
      { 2, 10, 30, 3.0 },
      { 2, 30, 10, 1.0 },
  };

  InfomapWrapper im(infomap::test::defaultFlags(
      "--directed --regularized --matchable-multilayer-ids 2 --no-infomap --two-level"));
  addMultilayerIntraLinks(im, intraLinks);

  im.run();

  checkRegularizedMultilayerFlow(im, analyticRegularizedMultilayerFlow(intraLinks));
}

TEST_CASE("Multilayer skip-absent-nodes flow matches analytic prior-strength sample [fast][core][flow]")
{
  // Node 2 is in both layers, node 3 only in layer 1 and node 4 only in layer 2.
  const std::vector<MultilayerIntraLink> intraLinks = {
      { 1, 1, 2, 2.0 },
      { 1, 2, 1, 1.0 },
      { 1, 1, 3, 1.0 },
      { 1, 3, 1, 1.0 },
      { 2, 1, 2, 1.0 },
      { 2, 2, 4, 3.0 },
      { 2, 4, 1, 1.0 },
  };

  InfomapWrapper im(infomap::test::defaultFlags(
      "--directed --regularized --multilayer-skip-absent-nodes --no-infomap --two-level"));
  addMultilayerIntraLinks(im, intraLinks);

  im.run();

  checkRegularizedMultilayerFlow(im, analyticRegularizedMultilayerFlow(intraLinks, true));
}

TEST_CASE("Multilayer skip-absent-nodes drops the state nodes a layer never observes [fast][core][flow]")
{
  const std::vector<MultilayerIntraLink> intraLinks = {
      { 1, 1, 2, 1.0 },
      { 1, 2, 1, 1.0 },
      { 1, 1, 3, 1.0 },
      { 1, 3, 1, 1.0 },
      { 2, 1, 2, 1.0 },
      { 2, 2, 1, 1.0 },
  };

  auto stateNodes = [&intraLinks](const std::string& extraFlags) {
    InfomapWrapper im(infomap::test::defaultFlags(
        "--directed --regularized --no-infomap --two-level " + extraFlags));
    addMultilayerIntraLinks(im, intraLinks);
    im.run();

    std::set<std::pair<unsigned int, unsigned int>> states;
    double sumFlow = 0.0;
    for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
      states.emplace((*it).layerId, (*it).physicalId);
      sumFlow += (*it).data.flow;
    }
    CHECK(sumFlow == doctest::Approx(1.0).epsilon(1e-12));
    return states;
  };

  // Node 3 has no link in layer 2. By default it is assumed to be there but
  // unobserved, so the prior still gives it a state node and a share of the flow.
  const auto assumedPresent = stateNodes("");
  CHECK(assumedPresent.size() == 6);
  CHECK(assumedPresent.count({ 2, 3 }) == 1);

  // Told that the absence is real, the prior spans only what layer 2 holds.
  const auto absent = stateNodes("--multilayer-skip-absent-nodes");
  CHECK(absent.size() == 5);
  CHECK(absent.count({ 2, 3 }) == 0);
}

TEST_CASE("Separate regularization strengths match analytic prior-strength samples [fast][core][flow]")
{
  const std::vector<MultilayerIntraLink> intraLinks = {
      { 1, 1, 2, 3.0 },
      { 1, 2, 1, 1.0 },
      { 1, 1, 3, 1.0 },
      { 1, 3, 1, 1.0 },
      { 2, 1, 2, 1.0 },
      { 2, 2, 3, 5.0 },
      { 2, 3, 1, 1.0 },
  };

  auto run = [&intraLinks](const std::string& extraFlags) {
    auto im = std::make_unique<InfomapWrapper>(infomap::test::defaultFlags(
        "--directed --regularized --no-infomap --two-level " + extraFlags));
    addMultilayerIntraLinks(*im, intraLinks);
    im->run();
    return im;
  };

  SUBCASE("no intra-layer prior")
  {
    auto im = run("--intra-regularization-strength 0");
    checkRegularizedMultilayerFlow(*im, analyticRegularizedMultilayerFlow(intraLinks, false, 0.0, 1.0));
  }

  SUBCASE("no inter-layer prior")
  {
    auto im = run("--inter-regularization-strength 0");
    checkRegularizedMultilayerFlow(*im, analyticRegularizedMultilayerFlow(intraLinks, false, 1.0, 0.0));
  }

  SUBCASE("the two strengths scale independently")
  {
    auto im = run("--intra-regularization-strength 0.5 --inter-regularization-strength 2");
    checkRegularizedMultilayerFlow(*im, analyticRegularizedMultilayerFlow(intraLinks, false, 0.5, 2.0));
  }

  SUBCASE("they compose with the overall strength")
  {
    // Halving both parts has to land exactly where halving the whole prior lands.
    auto split = run("--intra-regularization-strength 0.5 --inter-regularization-strength 0.5");
    auto whole = run("--regularization-strength 0.5");
    CHECK(split->codelength() == doctest::Approx(whole->codelength()).epsilon(1e-12));
  }
}

TEST_CASE("A zero intra-layer prior records no teleport flow at all [fast][core][flow]")
{
  // Every state node here has an observed out-link in its own layer.
  const std::vector<MultilayerIntraLink> connected = {
      { 1, 1, 2, 1.0 },
      { 1, 2, 1, 1.0 },
      { 2, 1, 2, 1.0 },
      { 2, 2, 1, 1.0 },
  };

  // Node 3 is in layer 1 only, and only as a link target, so its layer-1 state node has
  // no out-link to follow.
  auto dangling = connected;
  dangling.push_back({ 1, 1, 3, 1.0 });

  struct LayerTeleportUse {
    double sumFlow = 0.0;
    unsigned int nodesCarryingData = 0;
  };

  auto layerTeleportUse = [](const std::vector<MultilayerIntraLink>& links, const std::string& extraFlags) {
    InfomapWrapper im(infomap::test::defaultFlags(
        "--directed --regularized --multilayer-skip-absent-nodes --no-infomap --two-level " + extraFlags));
    addMultilayerIntraLinks(im, links);
    im.run();

    LayerTeleportUse use;
    for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
      const auto& layerFlows = (*it).layerTeleFlowData();
      if (!layerFlows.empty()) {
        ++use.nodesCarryingData;
      }
      for (const auto& layerFlow : layerFlows) {
        use.sumFlow += layerFlow.teleportFlow;
      }
    }
    return use;
  };

  auto sumLayerTeleportFlow = [&layerTeleportUse](const std::vector<MultilayerIntraLink>& links, const std::string& extraFlags) {
    return layerTeleportUse(links, extraFlags).sumFlow;
  };

  // With the prior on, every state node teleports a little.
  CHECK(sumLayerTeleportFlow(connected, "") > 1e-6);
  CHECK(sumLayerTeleportFlow(dangling, "") > 1e-6);

  // Turning the intra-layer prior off leaves nothing recorded to teleport. Where every
  // node has an out-link that is because none teleports at all; where one does not, its
  // flow is still passed on, but as unrecorded teleportation the codelength is not
  // charged for, exactly as in the unregularized flow model. Either way the per-layer
  // teleport bookkeeping is all zeros and could be skipped.
  CHECK(sumLayerTeleportFlow(connected, "--intra-regularization-strength 0") == doctest::Approx(0.0).epsilon(1e-12));
  CHECK(sumLayerTeleportFlow(dangling, "--intra-regularization-strength 0") == doctest::Approx(0.0).epsilon(1e-12));

  // Nothing teleports, so the per-layer bookkeeping is not built at all: no node carries
  // layer teleport data, which is what saves the allocation and the per-move lookup.
  CHECK(layerTeleportUse(connected, "").nodesCarryingData > 0);
  CHECK(layerTeleportUse(connected, "--intra-regularization-strength 0").nodesCarryingData == 0);
  CHECK(layerTeleportUse(dangling, "--intra-regularization-strength 0").nodesCarryingData == 0);
}

TEST_CASE("Skipping the per-layer teleport bookkeeping leaves the partition unchanged [fast][core][flow]")
{
  // Two triangles joined by a weak link, in two layers, so the optimizer has a real
  // choice of modules to make while the bookkeeping is skipped.
  std::vector<MultilayerIntraLink> intraLinks;
  for (unsigned int layer = 1; layer <= 2; ++layer) {
    for (const auto& pair : std::vector<std::pair<unsigned int, unsigned int>> {
             { 1, 2 }, { 2, 3 }, { 3, 1 }, { 4, 5 }, { 5, 6 }, { 6, 4 } }) {
      intraLinks.push_back({ layer, pair.first, pair.second, 1.0 });
      intraLinks.push_back({ layer, pair.second, pair.first, 1.0 });
    }
    intraLinks.push_back({ layer, 3, 4, 0.01 });
    intraLinks.push_back({ layer, 4, 3, 0.01 });
  }

  InfomapWrapper im(infomap::test::defaultFlags(
      "--directed --regularized --intra-regularization-strength 0 --two-level"));
  addMultilayerIntraLinks(im, intraLinks);

  CHECK_NOTHROW(im.run());
  infomap::test::checkRunSanity(im);
  // The optimizer still separates the two triangles without the teleport terms.
  CHECK(im.numTopModules() == 2);
}

TEST_CASE("Inner parallelization with regularized multilayer input falls back to stable serial optimization [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  ScopedOmpThreadCount ompThreads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  const std::vector<MultilayerIntraLink> intraLinks = {
      { 1, 1, 2, 2.0 },
      { 1, 2, 1, 1.0 },
      { 1, 1, 3, 1.0 },
      { 1, 3, 1, 1.0 },
      { 2, 1, 2, 1.0 },
      { 2, 2, 3, 3.0 },
      { 2, 3, 1, 1.0 },
  };

  auto runRegularizedMultilayer = [&intraLinks](const std::string& extraFlags) {
    InfomapWrapper im(infomap::test::defaultFlags("--directed --regularized --two-level " + extraFlags));
    addMultilayerIntraLinks(im, intraLinks);

    im.run();

    infomap::test::checkRunSanity(im);
    FlowRunResult result;
    result.modules = im.getModules(1, true);
    result.partition = infomap::test::canonicalPartition(result.modules);
    result.codelength = im.codelength();
    result.indexCodelength = im.getIndexCodelength();
    result.numTopModules = im.numTopModules();
    return result;
  };

  const auto serial = runRegularizedMultilayer("");
  const auto requestedInner = runRegularizedMultilayer("--inner-parallelization");

  CHECK(requestedInner.modules == serial.modules);
  CHECK(requestedInner.partition == serial.partition);
  CHECK(requestedInner.numTopModules == serial.numTopModules);
  infomap::test::checkApproxCodelength(requestedInner.codelength, serial.codelength, 1e-12);
  infomap::test::checkApproxCodelength(requestedInner.indexCodelength, serial.indexCodelength, 1e-12);
}
#else
TEST_CASE("Regularized multilayer flow requires compile-time feature [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--directed --regularized --no-infomap --two-level"));
  im.addMultilayerIntraLink(1, 1, 2, 1.0);
  im.addMultilayerIntraLink(1, 2, 1, 1.0);

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Regularized multilayer flow requires building with FEATURES=regularized-multilayer.",
      std::runtime_error);
}
#endif

TEST_CASE("Directed regularized codelength matches analytic multi-level tree [fast][core][flow][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags(
      "--directed --regularized --no-infomap --cluster-data "
      + infomap::test::clusterFixturePath("regularized_four_pairs_three_level.tree")));
  im.addLink(1, 2);
  im.addLink(2, 1);
  im.addLink(3, 4);
  im.addLink(4, 3);
  im.addLink(5, 6);
  im.addLink(6, 5);
  im.addLink(7, 8);
  im.addLink(8, 7);
  im.run();

  CHECK(im.numLevels() == 3);
  infomap::test::checkApproxCodelength(im.codelength(), 4.051270346886);
}

TEST_CASE("Inner parallelization remains runnable and numerically sane on the directed fixture [fast][core][flow][openmp]")
{
  const auto result = runDirectedFixture("--inner-parallelization");

  CHECK(result.numTopModules >= 1);
  CHECK(result.numTopModules <= 6);
  CHECK(result.partition.size() == result.numTopModules);
  CHECK(result.codelength >= result.indexCodelength);
}

TEST_CASE("Inner parallelization codelength matches its emitted partition [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  ScopedOmpThreadCount ompThreads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  checkInnerParallelPartitionCodelength("", "inner_parallel_partition_check.clu");
}

TEST_CASE("Inner parallelization codelength matches recorded-teleportation partition [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  ScopedOmpThreadCount ompThreads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  checkInnerParallelPartitionCodelength("--recorded-teleportation", "inner_parallel_recorded_partition_check.clu");
}

TEST_CASE("Inner parallelization is deterministic for a fixed seed and thread count [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  ScopedOmpThreadCount ompThreads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  const auto first = runDirectedFixture("--inner-parallelization");
  const auto second = runDirectedFixture("--inner-parallelization");

  CHECK(first.partition == second.partition);
  CHECK(first.codelength == doctest::Approx(second.codelength));
  CHECK(first.indexCodelength == doctest::Approx(second.indexCodelength));
}

TEST_CASE("Inner parallelization above the size threshold reports the codelength of its partition [fast][core][flow][openmp]")
{
  // The commit phase rechecks conflicting moves with the non-hoisted move delta,
  // which nothing else calls. #830/#904 was in the guard of that delta, so run
  // with and without each term it switches on.
  for (const std::string flags : {
           "",
           "--recorded-teleportation",
           "--entropy-corrected",
           "--preferred-number-of-modules 200",
           "--entropy-corrected --preferred-number-of-modules 200",
           "--recorded-teleportation --entropy-corrected --preferred-number-of-modules 200",
       }) {
    CAPTURE(flags);
    const auto result = runInnerParallelThresholdNetwork("--inner-parallelization --num-threads 4 " + flags);
#ifdef _OPENMP
    CHECK(result.threadsUsed > 1);
#else
    CHECK(result.threadsUsed == 1);
#endif

    InfomapWrapper check(infomap::test::defaultFlags("--directed --two-level --no-infomap " + flags));
    addInnerParallelThresholdNetwork(check);
    check.setInitialPartition(result.modules);
    check.run();

    CHECK(check.getModules() == result.modules);
    infomap::test::checkApproxCodelength(check.codelength(), result.codelength, 1e-9);
    infomap::test::checkApproxCodelength(check.getIndexCodelength(), result.indexCodelength, 1e-9);
  }
}

TEST_CASE("Inner parallelization with meta data falls back to stable serial optimization [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  ScopedOmpThreadCount ompThreads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  InfomapWrapper im(infomap::test::defaultFlags(
      "--inner-parallelization --meta-data " + infomap::test::fixturePath("meta/states.meta") + " --meta-data-rate 2"));
  infomap::test::readNetworkFixture(im, "states.net");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelength() > 0.0);
  CHECK(im.codelength() >= im.getIndexCodelength());
  CHECK(im.getMetaCodelength() >= 0.0);
  for (auto* leaf : im.leafNodes()) {
    CHECK_FALSE(leaf->metaData().empty());
    CHECK(leaf->metaData()[0] != -1);
  }
}

TEST_CASE("Inner parallelization with memory input falls back to stable serial optimization [fast][core][flow][openmp]")
{
#ifdef _OPENMP
  ScopedOmpThreadCount ompThreads(8);
  CHECK(omp_get_max_threads() > 1);
#endif

  InfomapWrapper im(infomap::test::defaultFlags("--inner-parallelization"));
  infomap::test::readNetworkFixture(im, "states.net");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.codelength() > 0.0);
  CHECK(im.codelength() >= im.getIndexCodelength());
}

TEST_CASE("Precomputed flow rejects first-order input without vertex flows [fast][core][flow][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  im.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net"));

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Missing node flow in input data. Should be passed as a third field under a *Vertices section.",
      infomap::InfomapError);
}

TEST_CASE("Precomputed flow rejects higher-order input without state flows [fast][core][flow][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  infomap::test::readNetworkFixture(im, "states.net");

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Missing node flow in input data. Should be passed as a third field under a *States section.",
      infomap::InfomapError);
}

TEST_CASE("Precomputed flow fixture remains runnable in C++ tests [fast][core][flow][columnar-contract]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  infomap::test::readNetworkFixture(im, "twotriangles_flow.net");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  infomap::test::checkApproxCodelength(im.codelength(), 1.889659695224);
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}});
}

TEST_CASE("multilayer-relax-to-self reproduces spread's codelength bit-exactly [fast][core][flow][multilayer][columnar-contract]")
{
  // Two communities (1,2,3) and (4,5,6) present in two layers, weakly bridged.
  std::vector<MultilayerIntraLink> intraLinks;
  const auto both = [&](unsigned int layer, unsigned int a, unsigned int b) {
    intraLinks.push_back({ layer, a, b, 1.0 });
    intraLinks.push_back({ layer, b, a, 1.0 });
  };
  for (unsigned int layer = 1; layer <= 2; ++layer) {
    both(layer, 1, 2);
    both(layer, 2, 3);
    both(layer, 1, 3);
    both(layer, 4, 5);
    both(layer, 5, 6);
    both(layer, 4, 6);
    both(layer, 3, 4); // bridge
  }

  const auto run = [&](const std::string& extra) {
    InfomapWrapper im(infomap::test::defaultFlags("--two-level -N 5 " + extra));
    addMultilayerIntraLinks(im, intraLinks);
    im.run();
    return std::make_pair(im.numTopModules(), im.codelength());
  };

  const auto spread = run("");
  const auto toself = run("--multilayer-relax-to-self");

  CHECK(spread.first == 2);
  CHECK(toself.first == spread.first);
  // The two-step relax flow reconstructs spread's flow, so the same partition
  // has bit-identical codelength on the compact network.
  infomap::test::checkApproxCodelength(toself.second, spread.second, 1e-9);
}

TEST_CASE("multilayer-relax-to-self matches spread on a single-node overlap [fast][core][flow][multilayer][columnar-contract]")
{
  // Two triangles sharing only the central ego node (1): layer 1 = {1,2,3},
  // layer 2 = {1,4,5}. The optimum splits the ego's two copies into the two
  // layer-local communities -- a genuine overlap (the point of state nodes).
  // Keeping the inter-layer link flow makes couple-to-self charge the ego's
  // layer switch exactly like spread (its copies are co-modular with their own
  // neighbours), so the same partition has bit-identical codelength.
  std::vector<MultilayerIntraLink> intraLinks;
  const auto both = [&](unsigned int layer, unsigned int a, unsigned int b) {
    intraLinks.push_back({ layer, a, b, 1.0 });
    intraLinks.push_back({ layer, b, a, 1.0 });
  };
  both(1, 1, 2);
  both(1, 1, 3);
  both(1, 2, 3); // layer 1 triangle {1,2,3}
  both(2, 1, 4);
  both(2, 1, 5);
  both(2, 4, 5); // layer 2 triangle {1,4,5}

  const auto run = [&](const std::string& extra) {
    InfomapWrapper im(infomap::test::defaultFlags("--two-level -N 20 " + extra));
    addMultilayerIntraLinks(im, intraLinks);
    im.run();
    return std::make_pair(im.numTopModules(), im.codelength());
  };

  const auto spread = run("");
  const auto toself = run("--multilayer-relax-to-self");

  CHECK(spread.first == 2); // the two triangles
  CHECK(toself.first == spread.first);
  infomap::test::checkApproxCodelength(toself.second, spread.second, 1e-9);
}

TEST_CASE("multilayer-relax-to-self gives spread's state flow for every teleportation mode (#1313, #1314) [fast][core][flow][multilayer]")
{
  // Node 3 is missing from layer 3. With recorded teleportation to links the
  // teleport weights must be spread's in-weights (#1313), and an explicit
  // inter-layer link to node 3 in layer 3 must be dropped as spread drops it,
  // not lose the flow that crosses it (#1314).
  const std::vector<MultilayerIntraLink> intraLinks {
    { 1, 1, 2, 1.0 }, { 1, 2, 3, 2.0 }, { 1, 3, 1, 1.0 }, { 1, 3, 4, 0.5 }, { 1, 4, 1, 1.0 },
    { 2, 1, 3, 1.0 }, { 2, 3, 2, 1.0 }, { 2, 2, 1, 3.0 }, { 2, 2, 4, 1.0 }, { 2, 4, 3, 2.0 },
    { 3, 4, 1, 1.0 }, { 3, 1, 2, 2.0 }, { 3, 2, 4, 1.0 },
  };
  const std::vector<MultilayerIntraLink> interLinks {
    { 1, 1, 2, 0.5 }, { 2, 1, 1, 0.5 }, { 2, 4, 1, 1.0 }, { 1, 3, 3, 0.3 }, // layer, node, target layer, weight
  };

  const auto flows = [&](const std::string& flags, bool withInterLinks) {
    InfomapWrapper im(infomap::test::defaultFlags("--no-infomap --two-level " + flags));
    addMultilayerIntraLinks(im, intraLinks);
    if (withInterLinks) {
      for (const auto& link : interLinks) {
        im.addMultilayerInterLink(link.layer, link.source, link.target, link.weight);
      }
    }
    im.run();
    std::map<std::pair<unsigned int, unsigned int>, double> stateFlows;
    for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
      stateFlows[{ it->layerId, it->physicalId }] += it->data.flow;
    }
    return std::make_pair(stateFlows, im.codelength());
  };

  for (const std::string model : { "--directed", "--flow-model undirected" }) {
    for (const std::string teleportation : { "", "--to-nodes", "--recorded-teleportation", "--recorded-teleportation --to-nodes" }) {
      for (const bool withInterLinks : { false, true }) {
        const auto flags = model + " " + teleportation;
        CAPTURE(flags);
        CAPTURE(withInterLinks);
        const auto spread = flows(flags, withInterLinks);
        const auto toself = flows(flags + " --multilayer-relax-to-self", withInterLinks);

        double sumFlow = 0.0;
        for (const auto& state : toself.first) {
          sumFlow += state.second;
        }
        CHECK(sumFlow == doctest::Approx(1.0).epsilon(1e-12));
        CHECK(toself.first.size() == spread.first.size());
        for (const auto& state : spread.first) {
          const auto it = toself.first.find(state.first);
          REQUIRE(it != toself.first.end());
          CHECK(it->second == doctest::Approx(state.second).epsilon(1e-10));
        }
        infomap::test::checkApproxCodelength(toself.second, spread.second, 1e-9);
      }
    }
  }
}

// Multilayer input with intra-layer links expands them to both directions and runs the directed
// model under undirected flow, and undirdir was replaced by that flow with only a -v note (#1233).
// It now warns. outdirdir and rawdir compute their own flow, and *Multilayer input keeps undirdir.
TEST_CASE("undirdir on multilayer input with intra-layer links warns that it has no effect (#1233) [fast][core][flow][multilayer]")
{
  const std::string warning = "The undirdir flow model has no effect on multilayer input with intra-layer links";
  const auto run = [](const std::string& model, const std::function<void(InfomapWrapper&)>& addInput) {
    std::ostringstream captured;
    double codelength = 0;
    {
      infomap::test::ScopedLogCapture capture(captured);
      // Not defaultFlags: it carries --silent, which mutes the warning under test.
      InfomapWrapper im("--seed 1 --num-trials 1 --no-file-output --flow-model " + model);
      addInput(im);
      im.run();
      codelength = im.codelength();
    }
    return std::make_pair(captured.str(), codelength);
  };
  const auto readFile = [](const std::string& path) {
    return [path](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath(path)); };
  };
  const auto intraLinks = [](InfomapWrapper& im) {
    addMultilayerIntraLinks(im, { { 1, 1, 2, 1.0 }, { 1, 2, 3, 1.0 }, { 2, 3, 1, 1.0 }, { 2, 1, 2, 2.0 } });
  };

  const std::vector<std::pair<std::string, std::function<void(InfomapWrapper&)>>> intraInputs {
    { "multilayer_intra_inter.net", readFile("examples/networks/multilayer_intra_inter.net") },
    { "multilayer_intra.net", readFile("examples/networks/multilayer_intra.net") },
    { "addMultilayerIntraLink", intraLinks },
  };
  for (const auto& input : intraInputs) {
    CAPTURE(input.first);
    const auto undirdir = run("undirdir", input.second);
    CHECK(undirdir.first.find(warning) != std::string::npos);
    CHECK(undirdir.second == run("undirected", input.second).second);
    for (const char* model : { "undirected", "directed", "outdirdir", "rawdir" }) {
      CAPTURE(model);
      CHECK(run(model, input.second).first.find(warning) == std::string::npos);
    }
  }

  CHECK(run("undirdir", readFile("examples/networks/multilayer.net")).first.find(warning) == std::string::npos);
}

// Undirected flow walks every state link in both directions, which a memory network's states
// cannot take: a backward step lands in a state with the wrong memory. State input that is not
// multilayer warns under the two models with undirected flow, and keeps running them (#1384).
TEST_CASE("undirected flow on state input that is not multilayer warns about memory networks (#1384) [fast][core][flow]")
{
  const std::string warning = "flow model walks every state link in both directions";
  const auto warns = [&warning](const std::string& model, const std::function<void(InfomapWrapper&)>& addInput) {
    std::ostringstream captured;
    {
      infomap::test::ScopedLogCapture capture(captured);
      // Not defaultFlags: it carries --silent, which mutes the warning under test.
      InfomapWrapper im("--seed 1 --num-trials 1 --no-file-output --flow-model " + model);
      addInput(im);
      im.run();
      CHECK(std::string(flowModelToString(im.flowModel)) == model);
    }
    return captured.str().find(warning) != std::string::npos;
  };
  const auto readFile = [](const std::string& path) {
    return [path](InfomapWrapper& im) { im.readInputData(infomap::test::repoPath(path)); };
  };
  // The second-order path 1 -> 2 -> 3 and back, with each state named by its memory.
  const auto memoryStates = [](InfomapWrapper& im) {
    im.addStateNode(12, 2);
    im.addStateNode(23, 3);
    im.addStateNode(32, 2);
    im.addStateNode(21, 1);
    im.addLink(12, 23);
    im.addLink(23, 32);
    im.addLink(32, 21);
  };

  for (const char* model : { "undirected", "undirdir" }) {
    CAPTURE(model);
    CHECK(warns(model, readFile("examples/networks/states.net")));
    CHECK(warns(model, memoryStates));
    CHECK_FALSE(warns(model, readFile("examples/networks/multilayer.net")));
    CHECK_FALSE(warns(model, readFile("examples/networks/twotriangles.net")));
  }
  for (const char* model : { "directed", "outdirdir", "rawdir" }) {
    CAPTURE(model);
    CHECK_FALSE(warns(model, readFile("examples/networks/states.net")));
  }
}

// The canonical bipartite shape: every link runs primary -> feature, as in
// examples/networks/bipartite.net. Several directed flow models degenerate on
// it (all-zero or non-finite node flow); the map equation is undefined there, so
// the run must fail rather than report a codelength.
void addCanonicalBipartiteLinks(InfomapWrapper& im)
{
  im.network().setBipartiteStartId(4);
  im.addLink(1, 4);
  im.addLink(2, 4);
  im.addLink(2, 5, 0.25);
  im.addLink(3, 5);
}

// The same links as addCanonicalBipartiteLinks plus one from the feature side back to
// the primary side, and without the bipartite declaration. A file whose links all run
// one way leaves the omitted dangling redistribution as a uniform scale factor, which
// the projection's renormalisation cancels; the back-link makes the dangling mass
// matter, which is what #899 is about.
void addBipartiteLinksWithFeedback(InfomapWrapper& im)
{
  im.addLink(1, 4);
  im.addLink(2, 4);
  im.addLink(2, 5, 0.25);
  im.addLink(3, 5);
  im.addLink(4, 1);
}

TEST_CASE("Degenerate flow is rejected instead of reported as a zero codelength [fast][core][flow]")
{
  // Each of these left node flow all-zero or NaN and still reported
  // "Best codelength 0" with a successful exit.
  for (const std::string& flags : { std::string("--directed --recorded-teleportation"),
                                    std::string("--directed --bipartite-teleportation"),
                                    std::string("--directed --recorded-teleportation --bipartite-teleportation") }) {
    InfomapWrapper im(infomap::test::defaultFlags("-N 1 --seed 7 " + flags));
    addCanonicalBipartiteLinks(im);

    CHECK_THROWS_AS(im.run(), infomap::InfomapError);
  }
}

TEST_CASE("Bipartite teleportation names the links it needs [fast][core][flow]")
{
  // The option's second step sends flow from the feature side back to the primary side
  // along explicit links. A canonical bipartite file has none, so that half was a no-op:
  // depending on the teleportation flags the result was zero flow, NaN, or -- with
  // --to-nodes -- the uniform teleport distribution with a plausible codelength and no
  // warning at all, i.e. flow that ignores the network (#899).
  for (const auto* flags : { "-N 1 --seed 7 --directed --bipartite-teleportation",
                             "-N 1 --seed 7 --directed --recorded-teleportation --bipartite-teleportation",
                             "-N 1 --seed 7 --directed --recorded-teleportation --to-nodes --bipartite-teleportation" }) {
    InfomapWrapper im(infomap::test::defaultFlags(flags));
    addCanonicalBipartiteLinks(im);

    try {
      im.run();
      FAIL("expected bipartite teleportation without feature -> primary links to be rejected");
    } catch (const infomap::InfomapError& e) {
      CHECK(e.code() == infomap::ExitCode::InputError);
      const std::string message(e.what());
      // Naming the missing precondition is the point: "degenerate flow" pointed at the
      // flow model when the cause is the input lacking return links.
      CHECK(message.find("--bipartite-teleportation needs links") != std::string::npos);
    }
  }
}

TEST_CASE("Bipartite teleportation runs when the feature side links back [fast][core][flow]")
{
  // One return link per feature node, so the two-step walk can reach both primary
  // nodes. With only one the walk can return to a single node, all flow concentrates
  // there and the codelength collapses to zero -- reported today by the non-convergence
  // and flow-sum warnings, which this PR also puts in the run report.
  InfomapWrapper im(infomap::test::defaultFlags("-N 1 --seed 7 --two-level --directed --bipartite-teleportation"));
  addCanonicalBipartiteLinks(im);
  im.addLink(4, 1);
  im.addLink(5, 3);

  CHECK_NOTHROW(im.run());
  infomap::test::checkRunSanity(im);
  CHECK(im.codelength() > 0.0);
  CHECK(im.network().flowConverged());
}

std::map<unsigned int, double> networkNodeFlows(InfomapWrapper& im)
{
  std::map<unsigned int, double> flows;
  for (const auto& node : im.network().nodes()) {
    flows[node.first] = node.second.flow;
  }
  return flows;
}

std::map<std::pair<unsigned int, unsigned int>, double> networkLinkFlows(InfomapWrapper& im)
{
  std::map<std::pair<unsigned int, unsigned int>, double> flows;
  const auto& network = im.network();
  network.forEachLink([&](unsigned int s, unsigned int t, double, double flow) {
    flows[{ network.nodeId(s), network.nodeId(t) }] = flow;
  });
  return flows;
}

TEST_CASE("Bipartite teleportation gives feature -> primary links the flow through the feature (#1310) [fast][core][flow]")
{
  // Primary nodes 1 and 2 both link to feature 3, which links back to 1 with weight 3 and
  // to 2 with weight 1, so every two-step move ends on 1 with probability 3/4. Recorded
  // teleportation to links teleports by the in-weight from features, also (3/4, 1/4), so
  // the primary flow is p = (3/4, 1/4) and feature 3 gets beta = 0.85 of it. Every link
  // carries the flow of its half of the move, doubled for Markov time 2: 2 * 0.85 * 3/4 =
  // 1.275 and 2 * 0.85 * 1/4 = 0.425 on both sides. The feature -> primary links had
  // zero flow, as the feature flow was reset before it was used.
  //
  // Unrecorded teleportation teleports by out-weight, (1/2, 1/2), so p = 0.15 * (1/2, 1/2) +
  // 0.85 * (3/4, 1/4) = (0.7125, 0.2875), and the node flow is one more move of it, (3/4,
  // 1/4). The links carry p without beta: 2 * 0.7125, 2 * 0.2875, 2 * 3/4 and 2 * 1/4.
  struct Expected {
    std::string flags;
    std::map<unsigned int, double> nodes;
    std::map<std::pair<unsigned int, unsigned int>, double> links;
  };
  for (const auto& expected : {
           Expected { "--recorded-teleportation", { { 1, 0.75 }, { 2, 0.25 }, { 3, 0.0 } }, { { { 1, 3 }, 1.275 }, { { 2, 3 }, 0.425 }, { { 3, 1 }, 1.275 }, { { 3, 2 }, 0.425 } } },
           Expected { "", { { 1, 0.75 }, { 2, 0.25 }, { 3, 0.0 } }, { { { 1, 3 }, 1.425 }, { { 2, 3 }, 0.575 }, { { 3, 1 }, 1.5 }, { { 3, 2 }, 0.5 } } },
       }) {
    CAPTURE(expected.flags);
    InfomapWrapper im(infomap::test::defaultFlags(std::string("-N 1 --seed 7 --directed --bipartite-teleportation ") + expected.flags));
    im.network().setBipartiteStartId(3);
    im.addLink(1, 3);
    im.addLink(2, 3);
    im.addLink(3, 1, 3.0);
    im.addLink(3, 2, 1.0);
    im.run();

    const auto nodes = networkNodeFlows(im);
    for (const auto& node : expected.nodes) {
      CAPTURE(node.first);
      CHECK(nodes.at(node.first) == doctest::Approx(node.second).epsilon(1e-10));
    }
    const auto links = networkLinkFlows(im);
    REQUIRE(links.size() == expected.links.size());
    for (const auto& link : expected.links) {
      CAPTURE(link.first.first);
      CAPTURE(link.first.second);
      CHECK(links.at(link.first) == doctest::Approx(link.second).epsilon(1e-10));
    }
  }
}

TEST_CASE("Unrecorded bipartite teleportation normalizes node flow with a dangling primary node (#1309) [fast][core][flow]")
{
  // The two-step walk runs 1 -> 2 -> 3, and 3 is dangling. Teleportation lands on nodes 1
  // and 2 in equal parts, and on node 3 too with --to-nodes, so either way p2 = p1 + beta * p1.
  // Unrecorded node flow is the flow of one more move, p1 into 2 and p2 into 3, normalized:
  // 1 / (2 + beta) and (1 + beta) / (2 + beta). It summed to p1 + p2 = 1 - p3. The links
  // carry the same flow, doubled for Markov time 2.
  const double beta = 0.85;
  const double flow2 = 1 / (2 + beta);
  const double flow3 = (1 + beta) / (2 + beta);
  for (const auto* flags : { "", "--to-nodes" }) {
    CAPTURE(flags);
    InfomapWrapper im(infomap::test::defaultFlags(std::string("-N 1 --seed 7 --directed --bipartite-teleportation ") + flags));
    im.network().setBipartiteStartId(4);
    im.addLink(1, 4);
    im.addLink(2, 5);
    im.addLink(4, 2);
    im.addLink(5, 3);
    im.run();

    const auto nodes = networkNodeFlows(im);
    CHECK(nodes.at(1) == doctest::Approx(0.0));
    CHECK(nodes.at(2) == doctest::Approx(flow2).epsilon(1e-10));
    CHECK(nodes.at(3) == doctest::Approx(flow3).epsilon(1e-10));
    const auto links = networkLinkFlows(im);
    CHECK(links.at({ 1, 4 }) == doctest::Approx(2 * flow2).epsilon(1e-10));
    CHECK(links.at({ 4, 2 }) == doctest::Approx(2 * flow2).epsilon(1e-10));
    CHECK(links.at({ 2, 5 }) == doctest::Approx(2 * flow3).epsilon(1e-10));
    CHECK(links.at({ 5, 3 }) == doctest::Approx(2 * flow3).epsilon(1e-10));
  }
}

TEST_CASE("Reaching tolerance on the last allowed iteration counts as converged [fast][core][flow]")
{
  // converged used to mean "the loop stopped before the iteration limit", so a run whose
  // final allowed iteration brought the error to zero was reported as a failure -- with
  // the warning to match. Since the outcome now reaches the run report and the Python
  // Result, a signal that cries wolf is worse than no signal (#899).
  InfomapWrapper im(infomap::test::defaultFlags("-N 1 --seed 7 --directed --max-flow-iterations 2"));
  im.addLink(1, 2);
  im.addLink(2, 3);
  im.addLink(3, 1);
  im.addLink(3, 4);
  im.addLink(4, 5);
  im.addLink(5, 3);

  im.run();

  REQUIRE(im.network().haveFlowConvergence());
  CHECK(im.network().flowIterations() == 2);
  CHECK(im.network().flowError() <= 1e-15);
  CHECK(im.network().flowConverged());
}

TEST_CASE("Degenerate flow reports an input error [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("-N 1 --seed 7 --directed --recorded-teleportation"));
  addCanonicalBipartiteLinks(im);

  try {
    im.run();
    FAIL("expected the degenerate flow to be rejected");
  } catch (const infomap::InfomapError& e) {
    CHECK(e.code() == infomap::ExitCode::InputError);
    // The message must name the quantity, so a user can tell this from a
    // parse failure or an empty network.
    CHECK(std::string(e.what()).find("flow") != std::string::npos);
  }
}

TEST_CASE("Node weights as flow do not hide a walk without flow [fast][core][flow]")
{
  // The node flow came from the weights, so the node-flow check passed, and the run reported
  // 1.185 bits for a walk that takes no link steps and teleports into no node (#1232).
  InfomapWrapper im(infomap::test::defaultFlags("-N 1 --seed 7 --directed --recorded-teleportation --use-node-weights-as-flow"));
  addCanonicalBipartiteLinks(im);

  try {
    im.run();
    FAIL("expected the walk without flow to be rejected");
  } catch (const infomap::InfomapError& e) {
    CHECK(e.code() == infomap::ExitCode::InputError);
    CHECK(std::string(e.what()).find("Degenerate flow") != std::string::npos);
  }
}

TEST_CASE("Node weights as flow do not hide a walk without flow on a link-free network [fast][core][flow]")
{
  // Recorded teleportation to links has no link to land on, so no node is entered, but the
  // node weights still gave the nodes flow. The link-free early return skipped the check above,
  // and the run reported 2 bits against a one-level codelength of 1 (#1253).
  InfomapWrapper im(infomap::test::defaultFlags("-N 1 --seed 7 --directed --recorded-teleportation --use-node-weights-as-flow"));
  im.addNode(1);
  im.addNode(2);

  try {
    im.run();
    FAIL("expected the walk without flow to be rejected");
  } catch (const infomap::InfomapError& e) {
    CHECK(e.code() == infomap::ExitCode::InputError);
    CHECK(std::string(e.what()).find("Degenerate flow") != std::string::npos);
  }
}

TEST_CASE("A link-free network keeps its trivial answer when the walk does not need to move [fast][core][flow]")
{
  // The #1253 check applies only when node weights give the nodes flow and teleportation is
  // recorded. Without either, the link-free network still reports its trivial zero-bit answer.
  for (const char* flags : { "--directed --recorded-teleportation", "--directed --use-node-weights-as-flow", "--directed --recorded-teleportation --to-nodes --use-node-weights-as-flow" }) {
    CAPTURE(flags);
    InfomapWrapper im(infomap::test::defaultFlags(std::string("-N 1 --seed 7 ") + flags));
    im.addNode(1);
    im.addNode(2);
    im.run();
    CHECK(im.codelength() <= im.getOneLevelCodelength() + 1e-10);
  }
}

TEST_CASE("skip-adjust-bipartite-flow keeps its documented unnormalized flow [fast][core][flow]")
{
  // The flag is documented as "Keep flow on bipartite nodes instead of
  // distributing it to primary nodes", so flow deliberately sums to less than
  // one here. That is an opt-out, not a defect: the run must still complete.
  InfomapWrapper im(infomap::test::defaultFlags("-N 1 --seed 7 --two-level --directed --skip-adjust-bipartite-flow"));
  addCanonicalBipartiteLinks(im);

  im.run();

  // Raised from 0.04164969778 when #899's dangling redistribution was restored: the
  // link flows now carry the 1/(1 - danglingRank) scaling the node flows already had,
  // so the exit flows -- and with them the module codelength -- are no longer deflated.
  infomap::test::checkApproxCodelength(im.codelength(), 0.2776646519);
}

TEST_CASE("Parallel self-links count once in the regularized prior [fast][core][flow]")
{
  // sumDegree() subtracted every self-link occurrence from the number of distinct links.
  // Two parallel self-loops on a node with no other links made the degree sum 0, so
  // regularized flow took the no-link shortcut and the node got a negative enter flow;
  // three wrapped the unsigned sum around and reported codelength 0. Found by igraph's
  // fuzzer (igraph/igraph#2940). Parallel links are aggregated, so they must give the
  // same result as one link of their summed weight.
  const auto run = [](const std::string& flags, const std::vector<double>& selfLinkWeights) {
    InfomapWrapper im(infomap::test::defaultFlags("--regularized --two-level -N 1 --seed 1 " + flags));
    for (const auto weight : selfLinkWeights) {
      im.addLink(1, 1, weight);
    }
    im.addLink(1, 2);
    im.addLink(2, 3);
    im.addLink(3, 4);
    im.addLink(4, 1);
    for (unsigned int node = 5; node <= 14; ++node) {
      im.addNode(node);
    }
    im.run();
    return std::make_pair(im.codelength(), im.getModules(1));
  };

  for (const std::string flags : { "--directed", "" }) {
    CAPTURE(flags);
    const auto single = run(flags, { 1.0 });
    for (const auto& parallel : { std::vector<double> { 0.5, 0.5 }, std::vector<double> { 0.25, 0.25, 0.5 } }) {
      CAPTURE(parallel.size());
      const auto aggregated = run(flags, parallel);
      infomap::test::checkApproxCodelength(aggregated.first, single.first, 1e-12);
      CHECK(aggregated.second == single.second);
    }
  }

  // The fuzzer's input: one node whose only links are two self-loops, plus isolated nodes.
  for (const std::string flags : { "--directed", "" }) {
    CAPTURE(flags);
    InfomapWrapper im(infomap::test::defaultFlags("--regularized --two-level -N 1 --seed 1 " + flags));
    im.addLink(0, 0, 0.609524);
    im.addLink(0, 0, 0.00952381);
    for (unsigned int node = 1; node <= 13; ++node) {
      im.addNode(node);
    }
    CHECK_NOTHROW(im.run());
    CHECK(std::isfinite(im.codelength()));
    CHECK(im.codelength() > 0);
  }
}

TEST_CASE("A link-free network keeps its trivial zero-flow result [fast][core][flow]")
{
  // No links means no flow to distribute and no structure to find: the trivial
  // partition is the correct answer, so the post-condition must not fire.
  InfomapWrapper isolatedNodes(infomap::test::defaultFlags("-N 1 --seed 7"));
  isolatedNodes.addNode(1);
  isolatedNodes.addNode(2);
  isolatedNodes.run();
  CHECK(isolatedNodes.numLeafNodes() == 2);

  InfomapWrapper singleNode(infomap::test::defaultFlags("-N 1 --seed 7"));
  singleNode.addNode(1);
  singleNode.run();
  CHECK(singleNode.numLeafNodes() == 1);

  // Variable Markov time has no links to rescale; its scale would be log2(0).
  InfomapWrapper variableMarkovTime(infomap::test::defaultFlags("-N 1 --seed 7 --variable-markov-time"));
  variableMarkovTime.addNode(1);
  variableMarkovTime.addNode(2);
  variableMarkovTime.run();
  CHECK(variableMarkovTime.numLeafNodes() == 2);
}

TEST_CASE("Directed bipartite flow matches the same links without the bipartite declaration [fast][core][flow]")
{
  // The bipartite declaration decides how flow is *reported* -- which nodes are the
  // feature side, and whether their flow is folded onto the primary side -- not how
  // the power iteration distributes it. With the fold opted out, the two runs must
  // therefore agree exactly. They did not (#899): the directed model finds its
  // dangling nodes as a prefix of the flow vector, an ordering bipartite input cannot
  // use because its feature side is identified by an index range, so the dangling
  // mass was never redistributed and the iteration converged somewhere else -- or
  // rather did not converge, exhausting --max-flow-iterations on a five-node network.
  //
  // Asserted as an equivalence rather than against pinned numbers, so the test states
  // the property instead of recording today's output.
  InfomapWrapper bipartite(infomap::test::defaultFlags("-N 1 --seed 7 --two-level --directed --skip-adjust-bipartite-flow"));
  bipartite.network().setBipartiteStartId(4);
  addBipartiteLinksWithFeedback(bipartite);
  bipartite.run();

  InfomapWrapper plain(infomap::test::defaultFlags("-N 1 --seed 7 --two-level --directed"));
  addBipartiteLinksWithFeedback(plain);
  plain.run();

  CHECK(bipartite.codelength() == doctest::Approx(plain.codelength()).epsilon(1e-10));

  std::map<unsigned int, double> plainFlow;
  for (auto it = plain.iterLeafNodes(); !it.isEnd(); ++it) {
    const auto& node = *it;
    plainFlow[node.physicalId] = node.data.flow;
  }

  unsigned int comparedNodes = 0;
  for (auto it = bipartite.iterLeafNodes(); !it.isEnd(); ++it) {
    const auto& node = *it;
    REQUIRE(plainFlow.count(node.physicalId) == 1);
    CHECK(node.data.flow == doctest::Approx(plainFlow[node.physicalId]).epsilon(1e-10));
    ++comparedNodes;
  }
  CHECK(comparedNodes == 5);
}

TEST_CASE("Ordinary bipartite runs are unaffected by the flow post-condition [fast][core][flow]")
{
  InfomapWrapper undirected(infomap::test::defaultFlags("-N 1 --seed 7"));
  addCanonicalBipartiteLinks(undirected);
  undirected.run();
  infomap::test::checkApproxCodelength(undirected.codelength(), 1.478406227);

  InfomapWrapper directed(infomap::test::defaultFlags("-N 1 --seed 7 --directed"));
  addCanonicalBipartiteLinks(directed);
  directed.run();
  // Raised from 0.4729743142 with #899's fix, for the same reason as above. The node
  // flows are unchanged here -- every link in this network runs one way, so the
  // omitted redistribution was a uniform scale the projection cancelled -- but the
  // link flows were uniformly too small, which is what the codelength reads.
  infomap::test::checkApproxCodelength(directed.codelength(), 1.2649313331);
}

// The columnar leaf takes each node's enter and exit flow from the flow calculator as stored:
// TOTAL boundary rates, the node's own link flow plus, under recorded teleportation, its
// teleport exit t(1 - w) and enter (T - t)w. The two sides have to agree on when that part is
// stored, and the stored value has to come from the links as scaled. Until the columnar levels
// switched to total rates the leaf stripped the teleport part instead, and this test pinned the
// link-only remainder: under rawdir with --recorded-teleportation the strip removed teleportation
// that was never added (exit 0.013 short on modular_wd.net), and under --regularized with
// --markov-time 2 a shortcut through node flow missed the scaled links by up to 0.036. Whatever
// the flow options, the leaf's enter and exit flow is its own link flow plus, exactly when the
// run records teleportation, its own teleport flow.
TEST_CASE("Columnar leaf: total enter and exit flow is the leaf's own link plus teleport flow under every flow option (#1194, #1189) [fast][core][flow][columnar-contract]")
{
  const auto check = [](const std::string& network, const std::string& flags, bool undirectedClustering) {
    INFO(network << " " << flags);
    InfomapWrapper im(infomap::test::defaultFlags("--columnar --num-trials 1 " + flags));
    im.readInputData(infomap::test::repoPath("examples/networks/" + network));
    im.run();
    const auto& leaf = im.columnarLeafInput();
    REQUIRE(leaf.n == static_cast<int>(im.numLeafNodes()));
    const bool tele = im.recordsTeleportation();
    double totalTele = 0.0;
    for (int i = 0; i < leaf.n; ++i)
      totalTele += leaf.teleFlow[i];
    for (int i = 0; i < leaf.n; ++i) {
      double outFlow = 0.0;
      double inFlow = 0.0;
      for (int e = leaf.outStart[i]; e < leaf.outStart[i + 1]; ++e)
        outFlow += leaf.outFlow[e];
      for (int e = leaf.inStart[i]; e < leaf.inStart[i + 1]; ++e)
        inFlow += leaf.inFlow[e];
      // An undirected link crosses either way with half its flow.
      double exitFlow = undirectedClustering ? (outFlow + inFlow) / 2 : outFlow;
      double enterFlow = undirectedClustering ? (outFlow + inFlow) / 2 : inFlow;
      if (tele) {
        exitFlow += leaf.teleFlow[i] * (1.0 - leaf.teleWeight[i]);
        enterFlow += (totalTele - leaf.teleFlow[i]) * leaf.teleWeight[i];
      }
      CHECK(std::abs(leaf.exit[i] - exitFlow) < 1e-12);
      CHECK(std::abs(leaf.enter[i] - enterFlow) < 1e-12);
    }
  };
  for (const char* flags : { "-d", "-d --recorded-teleportation", "-d --markov-time 2", "-d --regularized",
                             "-d --regularized --markov-time 2", "-d --regularized --markov-time 0.5",
                             "-d --regularized --variable-markov-time", "--flow-model undirdir --recorded-teleportation",
                             "--flow-model outdirdir --recorded-teleportation", "--flow-model rawdir --recorded-teleportation",
                             "--flow-model outdirdir --regularized", "--flow-model rawdir --regularized" })
    check("modular_wd.net", flags, false);
  for (const char* flags : { "", "--recorded-teleportation", "--regularized", "--regularized --markov-time 2", "--variable-markov-time" })
    check("modular_w.net", flags, true);

#if INFOMAP_FEATURE_REGULARIZED_MULTILAYER
  // Regularized multilayer flow teleports within the walker's layer (#828): no global teleport
  // flow, and each leaf carries one per-layer entry whose own teleport part is exit t(W_l - w)
  // and enter (T_l - t)w against its layer's totals. The link part is the leaf's out and in
  // flow even when clustering undirected: the regularized multilayer walk is computed on the
  // directed state network, so a link's flow differs between its two directions.
  const auto checkLayered = [](const std::string& network, const std::string& flags) {
    INFO(network << " " << flags);
    InfomapWrapper im(infomap::test::defaultFlags("--columnar --num-trials 1 " + flags));
    im.readInputData(infomap::test::repoPath("examples/networks/" + network));
    im.run();
    const auto& leaf = im.columnarLeafInput();
    REQUIRE(leaf.n == static_cast<int>(im.numLeafNodes()));
    REQUIRE(leaf.layerStart.size() == static_cast<std::size_t>(leaf.n) + 1);
    std::map<int, double> layerFlow, layerWeight;
    for (std::size_t k = 0; k < leaf.layerId.size(); ++k) {
      layerFlow[leaf.layerId[k]] += leaf.layerTeleFlow[k];
      layerWeight[leaf.layerId[k]] += leaf.layerTeleWeight[k];
    }
    CHECK(layerFlow.size() > 1);
    for (int i = 0; i < leaf.n; ++i) {
      CHECK(leaf.teleFlow[i] == 0.0);
      REQUIRE(leaf.layerStart[i + 1] - leaf.layerStart[i] == 1);
      const int k = leaf.layerStart[i];
      const int l = leaf.layerId[k];
      double outFlow = 0.0;
      double inFlow = 0.0;
      for (int e = leaf.outStart[i]; e < leaf.outStart[i + 1]; ++e)
        outFlow += leaf.outFlow[e];
      for (int e = leaf.inStart[i]; e < leaf.inStart[i + 1]; ++e)
        inFlow += leaf.inFlow[e];
      double exitFlow = outFlow;
      double enterFlow = inFlow;
      exitFlow += leaf.layerTeleFlow[k] * (layerWeight[l] - leaf.layerTeleWeight[k]);
      enterFlow += (layerFlow[l] - leaf.layerTeleFlow[k]) * leaf.layerTeleWeight[k];
      CHECK(std::abs(leaf.exit[i] - exitFlow) < 1e-12);
      CHECK(std::abs(leaf.enter[i] - enterFlow) < 1e-12);
    }
  };
  for (const char* network : { "multilayer_intra.net", "multilayer_intra_inter.net" }) {
    checkLayered(network, "--regularized");
    checkLayered(network, "--regularized --directed");
  }
#endif
}

// Markov time scales the link flow the map equation codes, not the flow of the network: the
// reported link flow is that of Markov time 1 (test/python/reference_flow.py). The branch
// stored the scaled flow in the network, so the output reported it, and --markov-time 0 left
// no link flow and failed as degenerate. The columnar leaf codes the scaled flow.
TEST_CASE("Columnar: Markov time scales the coded link flow and leaves the reported link flow at Markov time 1 [fast][core][flow][columnar-contract]")
{
  const auto links = [](const std::string& network, const std::string& flags) {
    InfomapWrapper im(infomap::test::defaultFlags("--columnar --num-trials 1 " + flags));
    im.readInputData(infomap::test::repoPath("examples/networks/" + network));
    im.run();
    return im.getLinks(true);
  };
  for (const char* network : { "modular_wd.net", "modular_w.net" }) {
    for (const char* model : { "-d", "", "--flow-model undirdir", "-d --regularized" }) {
      const auto reference = links(network, model);
      for (const char* markov : { "--markov-time 0.5", "--markov-time 2", "--markov-time 0", "--variable-markov-time" }) {
        INFO(network << " " << model << " " << markov);
        const auto scaled = links(network, std::string(model) + " " + markov);
        REQUIRE(scaled.size() == reference.size());
        for (const auto& [link, flow] : reference)
          CHECK(std::abs(scaled.at(link) - flow) < 1e-12);
      }
    }
  }

  InfomapWrapper im(infomap::test::defaultFlags("--columnar --num-trials 1 -d --markov-time 2"));
  im.readInputData(infomap::test::repoPath("examples/networks/modular_wd.net"));
  im.run();
  const auto reported = im.getLinks(true);
  const auto& leaf = im.columnarLeafInput();
  for (int i = 0; i < leaf.n; ++i) {
    for (int e = leaf.outStart[i]; e < leaf.outStart[i + 1]; ++e) {
      const auto source = im.network().nodeId(static_cast<unsigned int>(i));
      const auto target = im.network().nodeId(static_cast<unsigned int>(leaf.outTarget[e]));
      CHECK(std::abs(leaf.outFlow[e] - 2 * reported.at({ source, target })) < 1e-12);
    }
  }
}

// A directed cycle of 20 states over 10 physical nodes, link i -> i+1 of weight i. Under
// --regularized, the prior on a state network teleports with probability ~0.044 here, so
// the power iteration shrinks its error by only ~0.95 per step.
void addSlowCycle(InfomapWrapper& im)
{
  for (unsigned int s = 1; s <= 20; ++s) {
    im.addStateNode(s, (s - 1) % 10 + 1);
  }
  for (unsigned int s = 1; s <= 20; ++s) {
    im.addLink(s, s % 20 + 1, static_cast<double>(s));
  }
}

TEST_CASE("Regularized flow iterates to convergence with no default iteration cap (#1353) [fast][core][flow]")
{
  // The old default cap of 400 iterations stopped this flow at error 2.6e-9 and scored
  // partitions on a flow that was not stationary. On wikispeedia's state network the
  // same cap left an error of 1e-3, and a partition scored 4.408 bits instead of 3.207.
  InfomapWrapper im(infomap::test::defaultFlags("-N 1 --seed 7 --directed --regularized"));
  addSlowCycle(im);

  im.run();

  REQUIRE(im.network().haveFlowConvergence());
  CHECK(im.network().flowConverged());
  CHECK(im.network().flowError() <= 1e-15);
  // The plain iteration's measured contraction, which made it slow enough to accelerate.
  CHECK(im.network().flowAccelerated());
  CHECK(im.network().flowRate() > 0.95);
  CHECK(im.network().flowRate() < 1.0);
}

TEST_CASE("Flow stops at its rounding floor when the tolerance is out of reach (#1353) [fast][core][flow]")
{
  // With --flow-tolerance 0 the change per step never reaches tolerance; it stops falling
  // at the rounding floor instead. That is a converged flow, and the iteration has to end
  // there by itself now that no default cap ends it.
  InfomapWrapper im(infomap::test::defaultFlags("-N 1 --seed 7 --directed --flow-tolerance 0"));
  addSlowCycle(im);

  im.run();

  REQUIRE(im.network().haveFlowConvergence());
  CHECK(im.network().flowConverged());
  CHECK(im.network().flowError() <= 1e3 * std::numeric_limits<double>::epsilon());
  CHECK(im.network().flowIterations() < 1000);
  // Teleporting at 0.15 bounds the contraction by 0.85, too fast to accelerate.
  CHECK_FALSE(im.network().flowAccelerated());
  CHECK(im.network().flowRate() <= 0.85 + 1e-6);
}

} // namespace
