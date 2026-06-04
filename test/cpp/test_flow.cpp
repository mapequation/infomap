#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"
#include "io/Network.h"
#include "utils/FlowCalculator.h"
#include "utils/RegularizedMemoryFlowBuilder.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

AnalyticMultilayerFlow analyticRegularizedMultilayerFlow(const std::vector<MultilayerIntraLink>& intraLinks)
{
  std::map<unsigned int, unsigned int> layers;
  std::map<unsigned int, unsigned int> physicalIds;
  for (const auto& link : intraLinks) {
    layers.emplace(link.layer, layers.size());
    physicalIds.emplace(link.source, physicalIds.size());
    physicalIds.emplace(link.target, physicalIds.size());
  }

  std::vector<AnalyticStateFlow> states;
  states.reserve(layers.size() * physicalIds.size());
  for (const auto& layer : layers) {
    for (const auto& physicalId : physicalIds) {
      states.push_back({ layer.first, physicalId.first, 0.0 });
    }
  }

  const auto numStates = states.size();
  const auto numLayers = layers.size();
  const auto numPhysicalIds = physicalIds.size();
  const double intraPriorOutWeight = std::log(static_cast<double>(numPhysicalIds)) / static_cast<double>(numLayers);
  const double interLinkWeight = std::log(static_cast<double>(numLayers)) / static_cast<double>(numLayers);
  const double interOutWeight = interLinkWeight * static_cast<double>(numLayers);

  std::vector<double> sOut(numStates, 0.0);
  std::vector<std::vector<std::pair<unsigned int, double>>> intraProb(numStates);
  for (const auto& link : intraLinks) {
    const auto source = findStateIndex(states, link.layer, link.source);
    const auto target = findStateIndex(states, link.layer, link.target);
    sOut[source] += link.weight;
    intraProb[source].push_back({ target, link.weight });
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
    alpha[i] = sOut[i] == 0.0 ? 1.0 : intraPriorOutWeight / (intraPriorOutWeight + sOut[i]);
    alphaInter[i] = interOutWeight / (interOutWeight + sOut[i] + intraPriorOutWeight);
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
      for (const auto& layer : layers) {
        const auto target = findStateIndex(states, layer.first, states[source].physicalId);
        unrecordedInterFlow[target] += alphaInter[source] * stateFlow[source] / static_cast<double>(numLayers);
      }
    }

    for (unsigned int i = 0; i < numStates; ++i) {
      const auto layerIndex = layers.at(states[i].layer);
      layerTeleFlow[layerIndex] += alpha[i] * ((1.0 - alphaInter[i]) * stateFlow[i] + unrecordedInterFlow[i]);
    }

    for (unsigned int i = 0; i < numStates; ++i) {
      const auto layerIndex = layers.at(states[i].layer);
      nextFlow[i] += layerTeleFlow[layerIndex] / static_cast<double>(numPhysicalIds);
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

  for (auto it = im.iterLeafNodes(); !it.isEnd(); ++it) {
    const auto& node = *it;
    const auto expectedFlow = expectedStateFlows.at({ node.layerId, node.physicalId });
    CHECK(node.data.flow == doctest::Approx(expectedFlow).epsilon(1e-10));
  }
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

  std::array<std::array<double, 3>, 3> coefficients = { {
      { {
          1.0 - layerTeleportFlowCoefficients[0] / numPhysicalNodes - beta * presentBaseCoefficients[0],
          -layerTeleportFlowCoefficients[1] / numPhysicalNodes - beta * presentBaseCoefficients[1],
          -layerTeleportFlowCoefficients[2] / numPhysicalNodes - beta * presentBaseCoefficients[2],
      } },
      { {
          -layerTeleportFlowCoefficients[0] / numPhysicalNodes - beta / 2.0,
          1.0 - layerTeleportFlowCoefficients[1] / numPhysicalNodes - beta * presentBaseCoefficients[1] / 2.0,
          -layerTeleportFlowCoefficients[2] / numPhysicalNodes - beta * presentBaseCoefficients[2] / 2.0,
      } },
      { { 2.0, 4.0, 4.0 } },
  } };
  const auto solution = solve3x3(coefficients, { { 0.0, 0.0, 1.0 } });

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

struct MemoryBuilderInput {
  std::unordered_map<unsigned int, unsigned int> nodeIndexMap;
  std::vector<infomap::detail::FlowLink> observedLinks;
};

MemoryBuilderInput makeMemoryBuilderInput(const infomap::Network& network)
{
  MemoryBuilderInput input;
  unsigned int index = 0;
  for (const auto& node : network.nodes()) {
    input.nodeIndexMap[node.second.id] = index++;
  }
  for (const auto& source : network.nodeLinkMap()) {
    const auto sourceIndex = input.nodeIndexMap.at(source.first.id);
    for (const auto& target : source.second) {
      input.observedLinks.push_back({ sourceIndex, input.nodeIndexMap.at(target.first.id), target.second.weight });
    }
  }
  return input;
}

infomap::RegularizedMemoryFlowResult buildRegularizedMemoryFlow(infomap::Network& network, const std::string& extraFlags = "")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap " + extraFlags));
  network.setConfig(config);
  auto input = makeMemoryBuilderInput(network);
  return infomap::RegularizedMemoryFlowBuilder(network, config, input.nodeIndexMap, input.observedLinks).build();
}

double transitionProbability(const infomap::RegularizedMemoryFlowResult& result, unsigned int source, unsigned int target)
{
  for (const auto& transition : result.posterior.at(source)) {
    if (transition.first == target) {
      return transition.second;
    }
  }
  return 0.0;
}

unsigned int stateIndex(const infomap::Network& network, unsigned int stateId)
{
  return makeMemoryBuilderInput(network).nodeIndexMap.at(stateId);
}

std::unordered_map<unsigned int, unsigned int> stateIndexById(const infomap::Network& network)
{
  return makeMemoryBuilderInput(network).nodeIndexMap;
}

template <typename NetworkLike>
void addRegularizedMemoryPosteriorFixture(NetworkLike& network)
{
  network.addStateNode(12, 1); // s_1^2
  network.addStateNode(21, 2); // s_2^1
  network.addStateNode(31, 3); // s_3^1
  network.addStateNode(13, 1); // s_1^3
  network.addLink(12, 21, 2.0);
  network.addLink(12, 31, 1.0);
  network.addLink(21, 12, 3.0);
  network.addLink(31, 13, 4.0);
  network.addLink(13, 21, 1.0);
}

void checkSameRegularizedMemoryFlow(const infomap::RegularizedMemoryFlowResult& lhs, const infomap::RegularizedMemoryFlowResult& rhs)
{
  REQUIRE(lhs.posterior.size() == rhs.posterior.size());
  REQUIRE(lhs.nodeFlow.size() == rhs.nodeFlow.size());
  for (unsigned int i = 0; i < lhs.posterior.size(); ++i) {
    REQUIRE(lhs.posterior[i].size() == rhs.posterior[i].size());
    for (unsigned int j = 0; j < lhs.posterior[i].size(); ++j) {
      CHECK(lhs.posterior[i][j].first == rhs.posterior[i][j].first);
      CHECK(lhs.posterior[i][j].second == doctest::Approx(rhs.posterior[i][j].second).epsilon(1e-12));
    }
    CHECK(lhs.nodeFlow[i] == doctest::Approx(rhs.nodeFlow[i]).epsilon(1e-12));
  }
}

#if INFOMAP_FEATURE_REGULARIZED_HIGHER_ORDER
TEST_CASE("Regularized memory prior gamma matches two-node oracle [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network network(config);
  network.addStateNode(12, 1);
  network.addStateNode(21, 2);
  network.addLink(12, 21, 2.0);
  network.addLink(21, 12, 2.0);

  const auto result = buildRegularizedMemoryFlow(network);

  const double gamma = std::log(2.0) / 2.0;
  const double expectedObservedFlow = 0.5 * 2.0 / (2.0 + gamma);
  CHECK(result.observedLinkFlow[0] == doctest::Approx(expectedObservedFlow).epsilon(1e-12));
  CHECK(result.observedLinkFlow[1] == doctest::Approx(expectedObservedFlow).epsilon(1e-12));
}

TEST_CASE("Regularized memory posterior normalizes observed and prior support [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network network(config);
  addRegularizedMemoryPosteriorFixture(network);

  const auto result = buildRegularizedMemoryFlow(network);

  const double lambda = std::log(3.0) / 9.0;
  const double scale = 4.0 / 11.0;
  const double gammaTo2 = lambda * scale * 4.0 * 3.0 / 2.0;
  const double gammaTo3 = lambda * scale * 4.0 * 1.0 / 2.0;
  const double denominator = 3.0 + gammaTo2 + gammaTo3;

  CHECK(transitionProbability(result, stateIndex(network, 12), stateIndex(network, 21)) == doctest::Approx((2.0 + gammaTo2) / denominator).epsilon(1e-12));
  CHECK(transitionProbability(result, stateIndex(network, 12), stateIndex(network, 31)) == doctest::Approx((1.0 + gammaTo3) / denominator).epsilon(1e-12));
}

TEST_CASE("Regularized memory no-self-links controls physical self priors [fast][core][flow]")
{
  infomap::Config withSelf(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network withSelfNetwork(withSelf);
  withSelfNetwork.addStateNode(12, 1);
  withSelfNetwork.addStateNode(11, 1);
  withSelfNetwork.addStateNode(21, 2);
  withSelfNetwork.addLink(12, 11, 1.0);
  withSelfNetwork.addLink(12, 21, 1.0);
  withSelfNetwork.addLink(11, 21, 1.0);
  withSelfNetwork.addLink(21, 12, 1.0);
  auto withSelfResult = buildRegularizedMemoryFlow(withSelfNetwork);

  infomap::Config withoutSelf(infomap::test::defaultFlags("--directed --regularized --no-infomap --no-self-links"));
  infomap::Network withoutSelfNetwork(withoutSelf);
  withoutSelfNetwork.addStateNode(12, 1);
  withoutSelfNetwork.addStateNode(11, 1);
  withoutSelfNetwork.addStateNode(21, 2);
  withoutSelfNetwork.addLink(12, 11, 1.0);
  withoutSelfNetwork.addLink(12, 21, 1.0);
  withoutSelfNetwork.addLink(11, 21, 1.0);
  withoutSelfNetwork.addLink(21, 12, 1.0);
  auto withoutSelfResult = buildRegularizedMemoryFlow(withoutSelfNetwork, "--no-self-links");

  CHECK(transitionProbability(withSelfResult, stateIndex(withSelfNetwork, 12), stateIndex(withSelfNetwork, 11))
        > transitionProbability(withoutSelfResult, stateIndex(withoutSelfNetwork, 12), stateIndex(withoutSelfNetwork, 11)));
}

TEST_CASE("Regularized memory rejects dangling rows without valid second-order support [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network network(config);
  network.addStateNode(12, 1);
  network.addStateNode(21, 2);
  network.addStateNode(31, 3);
  network.addLink(12, 21, 1.0);
  network.addLink(21, 12, 1.0);

  CHECK_THROWS_WITH_AS(
      buildRegularizedMemoryFlow(network),
      "Regularized memory source state 31 at physical node 3 has no observed outgoing support and no valid second-order prior targets. Remove the sink state or run without --regularized.",
      std::runtime_error);
}

TEST_CASE("Regularized memory rejects conflicting inferred contexts [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network network(config);
  network.addStateNode(12, 1);
  network.addStateNode(21, 2);
  network.addStateNode(31, 3);
  network.addLink(12, 31, 1.0);
  network.addLink(21, 31, 1.0);

  CHECK_THROWS_WITH_AS(
      buildRegularizedMemoryFlow(network),
      "Regularized memory networks require valid single-step second-order state structure: state node 31 receives contexts 1 and 2.",
      std::runtime_error);
}

TEST_CASE("Regularized memory rejects duplicate physical-context targets [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network network(config);
  network.addStateNode(12, 1);
  network.addStateNode(13, 1);
  network.addStateNode(21, 2);
  network.addStateNode(201, 2);
  network.addLink(12, 21, 1.0);
  network.addLink(13, 201, 1.0);

  CHECK_THROWS_WITH_AS(
      buildRegularizedMemoryFlow(network),
      "Regularized memory networks require valid single-step second-order state structure: multiple state nodes map to physical/context pair (2, 1).",
      std::runtime_error);
}

TEST_CASE("Regularized memory accepts source states without inferred context [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network network(config);
  network.addStateNode(41, 4);
  network.addStateNode(14, 1);
  network.addStateNode(21, 2);
  network.addStateNode(32, 3);
  network.addStateNode(13, 1);
  network.addLink(41, 14, 1.0);
  network.addLink(14, 21, 1.0);
  network.addLink(21, 32, 1.0);
  network.addLink(32, 13, 1.0);
  network.addLink(13, 21, 1.0);

  CHECK_NOTHROW(buildRegularizedMemoryFlow(network));
}

TEST_CASE("Regularized memory ignores directed teleportation options [fast][core][flow]")
{
  infomap::Config baseConfig(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network baseNetwork(baseConfig);
  addRegularizedMemoryPosteriorFixture(baseNetwork);
  const auto baseResult = buildRegularizedMemoryFlow(baseNetwork);

  infomap::Config teleportConfig(infomap::test::defaultFlags("--directed --regularized --no-infomap --recorded-teleportation --to-nodes"));
  infomap::Network teleportNetwork(teleportConfig);
  addRegularizedMemoryPosteriorFixture(teleportNetwork);
  const auto teleportResult = buildRegularizedMemoryFlow(teleportNetwork, "--recorded-teleportation --to-nodes");

  checkSameRegularizedMemoryFlow(baseResult, teleportResult);
}

TEST_CASE("Regularized memory strength zero uses observed MLE on connected support [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap --regularization-strength 0"));
  infomap::Network network(config);
  addRegularizedMemoryPosteriorFixture(network);

  const auto result = buildRegularizedMemoryFlow(network, "--regularization-strength 0");

  CHECK(transitionProbability(result, stateIndex(network, 12), stateIndex(network, 21)) == doctest::Approx(2.0 / 3.0).epsilon(1e-12));
  CHECK(transitionProbability(result, stateIndex(network, 12), stateIndex(network, 31)) == doctest::Approx(1.0 / 3.0).epsilon(1e-12));
}

TEST_CASE("Regularized memory supports non-contiguous physical ids [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network network(config);
  network.addStateNode(1020, 10);
  network.addStateNode(2010, 20);
  network.addStateNode(3010, 30);
  network.addStateNode(1030, 10);
  network.addLink(1020, 2010, 2.0);
  network.addLink(1020, 3010, 1.0);
  network.addLink(2010, 1020, 3.0);
  network.addLink(3010, 1030, 4.0);
  network.addLink(1030, 2010, 1.0);

  const auto result = buildRegularizedMemoryFlow(network);

  CHECK(transitionProbability(result, stateIndex(network, 1020), stateIndex(network, 2010)) > 0.0);
  CHECK(transitionProbability(result, stateIndex(network, 1020), stateIndex(network, 3010)) > 0.0);
}

TEST_CASE("Regularized memory tiny prior strength approaches observed MLE [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap --regularization-strength 1e-12"));
  infomap::Network network(config);
  addRegularizedMemoryPosteriorFixture(network);

  const auto result = buildRegularizedMemoryFlow(network, "--regularization-strength 1e-12");

  CHECK(transitionProbability(result, stateIndex(network, 12), stateIndex(network, 21)) == doctest::Approx(2.0 / 3.0).epsilon(1e-10));
  CHECK(transitionProbability(result, stateIndex(network, 12), stateIndex(network, 31)) == doctest::Approx(1.0 / 3.0).epsilon(1e-10));
}

TEST_CASE("Regularized memory handles zero-degree physical nodes without NaN [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network network(config);
  addRegularizedMemoryPosteriorFixture(network);
  network.addPhysicalNode(99);

  const auto result = buildRegularizedMemoryFlow(network);

  for (const auto flow : result.nodeFlow) {
    CHECK(std::isfinite(flow));
  }
  for (const auto& transitions : result.posterior) {
    for (const auto& transition : transitions) {
      CHECK(std::isfinite(transition.second));
    }
  }
}

TEST_CASE("Regularized memory physical flow is the sum of state flows [fast][core][flow]")
{
  infomap::Config config(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  infomap::Network network(config);
  addRegularizedMemoryPosteriorFixture(network);

  const auto result = buildRegularizedMemoryFlow(network);
  const auto indices = stateIndexById(network);

  std::map<unsigned int, double> physicalFlow;
  for (const auto& node : network.nodes()) {
    physicalFlow[node.second.physicalId] += result.nodeFlow[indices.at(node.second.id)];
  }

  const double totalPhysicalFlow = std::accumulate(
      physicalFlow.begin(),
      physicalFlow.end(),
      0.0,
      [](double sum, const auto& flow) { return sum + flow.second; });

  CHECK(totalPhysicalFlow == doctest::Approx(1.0).epsilon(1e-12));
  for (const auto& flow : physicalFlow) {
    CHECK(flow.second > 0.0);
  }
}

TEST_CASE("Regularized memory dispatch rejects unsupported memory variants [fast][core][flow]")
{
  InfomapWrapper undirected(infomap::test::defaultFlags("--flow-model undirected --regularized --no-infomap"));
  undirected.addStateNode(12, 1);
  undirected.addStateNode(21, 2);
  undirected.addLink(12, 21, 1.0);
  undirected.addLink(21, 12, 1.0);
  CHECK_THROWS_WITH_AS(
      undirected.run(),
      "Regularized undirected memory networks are not supported yet; use directed flow or run without --regularized.",
      std::runtime_error);

  InfomapWrapper bipartite(infomap::test::defaultFlags("--directed --regularized --no-infomap"));
  bipartite.addStateNode(12, 1);
  bipartite.addStateNode(21, 2);
  bipartite.addLink(12, 21, 1.0);
  bipartite.addLink(21, 12, 1.0);
  bipartite.setBipartiteStartId(21);
  CHECK_THROWS_WITH_AS(
      bipartite.run(),
      "Regularized bipartite memory networks are not supported.",
      std::runtime_error);
}

TEST_CASE("Regularized memory keeps prior links hidden from output links [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--directed --regularized --no-infomap --two-level"));
  addRegularizedMemoryPosteriorFixture(im);

  im.run();

  CHECK(im.network().haveEffectiveLinkMap());
  CHECK(im.getLinks(false).count({ 13, 31 }) == 0);

  const auto sourceIt = im.network().effectiveLinkMap().find(infomap::StateNetwork::StateNode(13));
  REQUIRE(sourceIt != im.network().effectiveLinkMap().end());

  const auto targetIt = sourceIt->second.find(infomap::StateNetwork::StateNode(31));
  REQUIRE(targetIt != sourceIt->second.end());
  CHECK(targetIt->second.weight > 0.0);
  CHECK(targetIt->second.flow > 0.0);
}
#else
TEST_CASE("Regularized memory flow requires compile-time feature [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--directed --regularized --no-infomap --two-level"));
  addRegularizedMemoryPosteriorFixture(im);

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Regularized higher-order flow requires building with FEATURES=regularized-higher-order.",
      std::runtime_error);
}
#endif

TEST_CASE("Recorded teleportation stays stable across trial counts for the directed fixture [fast][core][flow]")
{
  const auto oneTrial = runDirectedFixture("--recorded-teleportation --num-trials 1");
  const auto twoTrials = runDirectedFixture("--recorded-teleportation --num-trials 2");

  CHECK(oneTrial.partition == std::vector<std::vector<unsigned int>> { { 1, 2, 3 }, { 4, 5, 6 } });
  CHECK(oneTrial.partition == twoTrials.partition);
  CHECK(oneTrial.codelength == doctest::Approx(twoTrials.codelength));
  CHECK(oneTrial.indexCodelength == doctest::Approx(twoTrials.indexCodelength));
}

TEST_CASE("Directed to-nodes teleportation keeps the expected coarse partition [fast][core][flow]")
{
  const auto result = runDirectedFixture("--to-nodes");

  CHECK(result.numTopModules == 2);
  CHECK(result.partition == std::vector<std::vector<unsigned int>> { { 1, 2, 3 }, { 4, 5, 6 } });
}

TEST_CASE("Directed regularization remains numerically sane on the teleportation fixture [fast][core][flow]")
{
  const auto result = runDirectedFixture("--regularized");

  CHECK(result.numTopModules == 1);
  CHECK(result.partition == std::vector<std::vector<unsigned int>> { { 1, 2, 3, 4, 5, 6 } });
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
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3, 4, 5, 6 } });
  infomap::test::checkApproxCodelength(im.codelength(), 2.575767408, 1e-9);
}

TEST_CASE("Directed regularized codelength matches analytic multi-level tree [fast][core][flow]")
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

#if INFOMAP_FEATURE_REGULARIZED_HIGHER_ORDER
TEST_CASE("Regularized multilayer flow supports non-dense matchable state ids [fast][core][flow]")
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

TEST_CASE("Regularized multilayer ego flow matches analytic prior-strength sample [fast][core][flow]")
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

TEST_CASE("Regularized multilayer asymmetric flow matches analytic prior-strength sample [fast][core][flow]")
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

TEST_CASE("Regularized multilayer matchable state-id flow matches analytic prior-strength sample [fast][core][flow]")
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
TEST_CASE("Regularized higher-order multilayer flow requires compile-time feature [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--directed --regularized --no-infomap --two-level"));
  im.addMultilayerIntraLink(1, 1, 2, 1.0);
  im.addMultilayerIntraLink(1, 2, 1, 1.0);

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Regularized higher-order flow requires building with FEATURES=regularized-higher-order.",
      std::runtime_error);
}
#endif

TEST_CASE("Directed regularized codelength matches analytic multi-level tree [fast][core][flow]")
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
    CHECK_FALSE(leaf->metaData.empty());
    CHECK(leaf->metaData[0] != -1);
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
      std::runtime_error);
}

TEST_CASE("Precomputed flow rejects higher-order input without state flows [fast][core][flow][parser]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  infomap::test::readNetworkFixture(im, "states.net");

  CHECK_THROWS_WITH_AS(
      im.run(),
      "Missing node flow in input data. Should be passed as a third field under a *States section.",
      std::runtime_error);
}

TEST_CASE("Precomputed flow fixture remains runnable in C++ tests [fast][core][flow]")
{
  InfomapWrapper im(infomap::test::defaultFlags("--flow-model precomputed"));
  infomap::test::readNetworkFixture(im, "twotriangles_flow.net");

  im.run();

  infomap::test::checkRunSanity(im);
  CHECK(im.numTopModules() == 2);
  CHECK(im.numLevels() == 2);
  infomap::test::checkApproxCodelength(im.codelength(), 1.889659695224);
  infomap::test::checkCanonicalPartition(im, { { 1, 2, 3 }, { 4, 5, 6 } });
}

} // namespace
