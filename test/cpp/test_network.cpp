#include "vendor/doctest.h"

#include "io/Config.h"
#include "io/Network.h"

#include "TestUtils.h"

namespace {

using infomap::Config;
using infomap::Network;
using infomap::StateNetwork;

class InspectableNetwork : public Network {
public:
  explicit InspectableNetwork(const Config& config) : Network(config) {}

  unsigned int numSelfLinksFound() const { return m_numSelfLinksFound; }
  unsigned int numAggregatedLinks() const { return m_numAggregatedLinks; }
  double totalLinkWeightAdded() const { return m_totalLinkWeightAdded; }
  unsigned int numLinksIgnoredByWeightThreshold() const { return m_numLinksIgnoredByWeightThreshold; }
  double totalLinkWeightIgnored() const { return m_totalLinkWeightIgnored; }
};

std::map<std::pair<unsigned int, unsigned int>, double> linksById(const Network& network)
{
  std::map<std::pair<unsigned int, unsigned int>, double> links;
  for (const auto& sourceLinks : network.nodeLinkMap()) {
    const auto source = sourceLinks.first.id;
    for (const auto& targetLink : sourceLinks.second) {
      links[std::make_pair(source, targetLink.first.id)] = targetLink.second.weight;
    }
  }
  return links;
}

void checkSameNetwork(InspectableNetwork& actual, InspectableNetwork& expected)
{
  CHECK(actual.numNodes() == expected.numNodes());
  CHECK(actual.numPhysicalNodes() == expected.numPhysicalNodes());
  CHECK(actual.numLinks() == expected.numLinks());
  CHECK(actual.sumLinkWeight() == doctest::Approx(expected.sumLinkWeight()));
  CHECK(actual.numSelfLinks() == expected.numSelfLinks());
  CHECK(actual.numSelfLinksFound() == expected.numSelfLinksFound());
  CHECK(actual.sumSelfLinkWeight() == doctest::Approx(expected.sumSelfLinkWeight()));
  CHECK(actual.sumDegree() == expected.sumDegree());
  CHECK(actual.sumWeightedDegree() == doctest::Approx(expected.sumWeightedDegree()));
  CHECK(actual.numAggregatedLinks() == expected.numAggregatedLinks());
  CHECK(actual.totalLinkWeightAdded() == doctest::Approx(expected.totalLinkWeightAdded()));
  CHECK(actual.numLinksIgnoredByWeightThreshold() == expected.numLinksIgnoredByWeightThreshold());
  CHECK(actual.totalLinkWeightIgnored() == doctest::Approx(expected.totalLinkWeightIgnored()));
  CHECK(actual.outWeights() == expected.outWeights());
  CHECK(linksById(actual) == linksById(expected));
}

TEST_CASE("Network aggregates duplicate links [fast][core]")
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

TEST_CASE("Network bulk links match repeated addLink counters [fast][core]")
{
  Config config;
  config.silent = true;
  config.weightThreshold = 0.5;

  std::vector<StateNetwork::LinkInput> links;
  links.emplace_back(3, 4, 2.0);
  links.emplace_back(1, 2, 1.0);
  links.emplace_back(1, 2, 2.0);
  links.emplace_back(2, 2, 5.0);
  links.emplace_back(4, 4, 0.4);
  links.emplace_back(7, 8, 0.0);
  links.emplace_back(9, 9, -1.0);
  links.emplace_back(1, 2, 3.0);

  InspectableNetwork expected(config);
  expected.addLink(1, 2, 10.0);
  expected.addLink(10, 11, 1.0);
  for (const auto& link : links) {
    expected.addLink(link.source, link.target, link.weight);
  }

  InspectableNetwork actual(config);
  actual.addLink(1, 2, 10.0);
  actual.addLink(10, 11, 1.0);
  actual.addLinksBulk(links);

  checkSameNetwork(actual, expected);
}

TEST_CASE("Network bulk links match repeated addLink with no self-links [fast][core]")
{
  Config config;
  config.silent = true;
  config.noSelfLinks = true;

  std::vector<StateNetwork::LinkInput> links;
  links.emplace_back(1, 1, 2.0);
  links.emplace_back(1, 2, 1.0);
  links.emplace_back(1, 2, 4.0);
  links.emplace_back(3, 3, 5.0);
  links.emplace_back(2, 3, 1.0);

  InspectableNetwork expected(config);
  for (const auto& link : links) {
    expected.addLink(link.source, link.target, link.weight);
  }

  InspectableNetwork actual(config);
  actual.addLinksBulk(links);

  checkSameNetwork(actual, expected);
}

TEST_CASE("Network reads states fixture as higher-order input [fast][core]")
{
  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(infomap::test::repoPath("test/fixtures/networks/states.net"));

  CHECK(network.haveMemoryInput());
  CHECK(network.numNodes() == 6);
  CHECK(network.numPhysicalNodes() == 5);
}

TEST_CASE("Network tracks deterministic multilayer state ids [fast][core][crash]")
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

TEST_CASE("Network rejects malformed multilayer fixture lines [fast][core][parser]")
{
  Config config;
  config.silent = true;
  Network network(config);

  CHECK_THROWS_WITH_AS(
      network.readInputData(infomap::test::repoPath("test/fixtures/networks/invalid_multilayer.net")),
      "Can't parse multilayer link data from line '1 1 broken'",
      std::runtime_error);
}

} // namespace
