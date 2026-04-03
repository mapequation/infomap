#include "vendor/doctest.h"

#include "io/Config.h"
#include "io/Network.h"

#include "TestUtils.h"

namespace {

using infomap::Config;
using infomap::Network;

TEST_CASE("Network aggregates duplicate links")
{
  Config config;
  config.silent = true;
  Network network(config);

  CHECK(network.addLink(1, 2, 1.0));
  CHECK_FALSE(network.addLink(1, 2, 2.5));

  CHECK(network.numNodes() == 2);
  CHECK(network.numLinks() == 1);
  CHECK(network.sumLinkWeight() == doctest::Approx(3.5));
  CHECK(network.outWeights().at(1) == doctest::Approx(3.5));
}

TEST_CASE("Network reads states fixture as higher-order input")
{
  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(infomap::test::repoPath("examples/networks/states.net"));

  CHECK(network.haveMemoryInput());
  CHECK(network.numNodes() == 6);
  CHECK(network.numPhysicalNodes() == 5);
}

TEST_CASE("Network tracks deterministic multilayer state ids")
{
  Config config;
  config.silent = true;
  config.matchableMultilayerIds = 3;
  Network network(config);

  network.addMultilayerLink(2, 1, 1, 2, 1.0);
  network.addMultilayerLink(1, 2, 2, 1, 1.0);

  CHECK(network.haveMemoryInput());
  CHECK(network.numNodes() == 2);
  CHECK(network.nodes().count(10) == 1);
  CHECK(network.nodes().count(17) == 1);
}

} // namespace
