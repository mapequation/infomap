#include "vendor/doctest.h"

#include "Infomap.h"

#include "TestUtils.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using infomap::InfomapWrapper;

struct FlowRunResult {
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

double plogp(double value)
{
  return value <= 0.0 ? 0.0 : value * std::log2(value);
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

  infomap::test::checkRunSanity(im);
  return {
      infomap::test::canonicalPartition(im.getModules()),
      im.codelength(),
      im.getIndexCodelength(),
      im.numTopModules(),
  };
}

TEST_CASE("Recorded teleportation stays stable across trial counts for the directed fixture [fast][core][flow]")
{
  const auto oneTrial = runDirectedFixture("--recorded-teleportation --num-trials 1");
  const auto twoTrials = runDirectedFixture("--recorded-teleportation --num-trials 2");

  CHECK(oneTrial.partition == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
  CHECK(oneTrial.partition == twoTrials.partition);
  CHECK(oneTrial.codelength == doctest::Approx(twoTrials.codelength));
  CHECK(oneTrial.indexCodelength == doctest::Approx(twoTrials.indexCodelength));
}

TEST_CASE("Directed to-nodes teleportation keeps the expected coarse partition [fast][core][flow]")
{
  const auto result = runDirectedFixture("--to-nodes");

  CHECK(result.numTopModules == 2);
  CHECK(result.partition == std::vector<std::vector<unsigned int>>{{1, 2, 3}, {4, 5, 6}});
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
  infomap::test::checkCanonicalPartition(im, {{1, 2, 3}, {4, 5, 6}});
}

} // namespace
