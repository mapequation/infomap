#include "vendor/doctest.h"

#include "io/Config.h"
#include "io/Network.h"
#include "io/NetworkInputParser.h"

#include "TestUtils.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

namespace {

using infomap::Config;
using infomap::Network;

const infomap::input::NetworkInputOptions defaultInputOptions {};

struct FakeInputSink {
  bool undirectedFlow = true;
  unsigned int matchableIds = 0;
  unsigned int bipartiteStartId = 0;
  bool fileInput = false;
  bool parsed = false;

  std::vector<infomap::input::ParsedVertex> vertices;
  std::vector<infomap::input::ParsedStateNode> stateNodes;
  std::vector<infomap::input::ParsedLink> links;
  std::vector<infomap::input::ParsedMultilayerLink> multilayerLinks;
  std::vector<infomap::input::ParsedMultilayerIntraLink> intraLinks;
  std::vector<infomap::input::ParsedMultilayerInterLink> interLinks;
  std::vector<infomap::input::ParsedLink> bipartiteLinks;
  std::vector<std::vector<int>> metaDataRows;

  unsigned int numPhysicalNodes() const { return vertices.size(); }
  unsigned int numLinks() const { return links.size(); }
  unsigned int numIntraLayerLinks() const { return intraLinks.size() + countExplicitIntraLinks(); }
  unsigned int numInterLayerLinks() const { return interLinks.size() + countExplicitInterLinks(); }
  unsigned int numLayerLinks() const { return numIntraLayerLinks() + numInterLayerLinks(); }
  unsigned int numLayers() const { return 0; }
  unsigned int numMetaDataColumns() const { return metaDataRows.empty() ? 0 : metaDataRows.front().size(); }
  unsigned int numMetaDataRows() const { return metaDataRows.size(); }

  void onFileInput() { fileInput = true; }
  void onNetworkParsed() { parsed = true; }
  void onPhysicalNode(const infomap::input::ParsedVertex& vertex) { vertices.push_back(vertex); }
  void onStateNode(const infomap::input::ParsedStateNode& stateNode) { stateNodes.push_back(stateNode); }
  void onLink(const infomap::input::ParsedLink& link) { links.push_back(link); }
  void onMultilayerLink(const infomap::input::ParsedMultilayerLink& link) { multilayerLinks.push_back(link); }
  void onMultilayerIntraLink(const infomap::input::ParsedMultilayerIntraLink& link) { intraLinks.push_back(link); }
  void onMultilayerInterLink(const infomap::input::ParsedMultilayerInterLink& link) { interLinks.push_back(link); }
  void onBipartiteStart(unsigned int startId) { bipartiteStartId = startId; }
  void onBipartiteLink(const std::string&, const infomap::input::ParsedLink& link) { bipartiteLinks.push_back(link); }
  void onMetaData(unsigned int, const std::vector<int>& metaData) { metaDataRows.push_back(metaData); }

private:
  unsigned int countExplicitIntraLinks() const
  {
    unsigned int count = 0;
    for (const auto& link : multilayerLinks) {
      if (link.sourceLayer == link.targetLayer) {
        ++count;
      }
    }
    return count;
  }

  unsigned int countExplicitInterLinks() const
  {
    return multilayerLinks.size() - countExplicitIntraLinks();
  }
};

TEST_CASE("Network aggregates duplicate links after finalize [fast][core]")
{
  Config config;
  config.silent = true;
  Network network(config);

  // Deferred dedup (mode A): addLink reports "accepted" (passed filters), not
  // "new-unique" -- aggregation happens in finalizeLinks().
  CHECK(network.addLink(1, 2, 1.0));
  CHECK(network.addLink(1, 2, 2.5));

  network.finalizeLinks();

  CHECK(network.numNodes() == 2);
  CHECK(network.numLinks() == 1); // aggregated
  CHECK(network.numAggregatedLinks() == 1);
  CHECK(network.sumLinkWeight() == doctest::Approx(3.5)); // occurrence sum (eager)
  // Aggregated CSR weight read straight from the consumed store (mode A no longer
  // derives outWeights).
  double mergedWeight = -1.0;
  network.forEachLink([&](unsigned int, unsigned int, double w, double&) { mergedWeight = w; });
  CHECK(mergedWeight == doctest::Approx(3.5));
}

TEST_CASE("Mode-A map APIs work: outWeights / removeLink / undirectedToDirected [fast][core][csr]")
{
  Config config;
  config.silent = true;
  Network network(config);
  network.addLink(1, 2, 1.0);
  network.addLink(1, 3, 2.0);
  network.addLink(2, 3, 4.0);
  network.finalizeLinks();

  // outWeights() is derived on demand for the flat-buffer (mode-A) build.
  CHECK(network.outWeights().at(1) == doctest::Approx(3.0));
  CHECK(network.outWeights().at(2) == doctest::Approx(4.0));

  // removeLink materializes the nested map from CSR and removes the link.
  CHECK(network.numLinks() == 3);
  CHECK(network.removeLink(1, 2));
  CHECK(network.numLinks() == 2);
  CHECK_FALSE(network.removeLink(1, 2)); // already gone
  CHECK(network.outWeights().at(1) == doctest::Approx(2.0)); // 3.0 - 1.0
}

TEST_CASE("undirectedToDirected expands a mode-A network [fast][core][csr]")
{
  Config config("--two-level --silent");
  Network network(config);
  network.addLink(1, 2, 1.0);
  network.addLink(2, 3, 2.0);
  network.finalizeLinks();
  CHECK(network.numLinks() == 2);

  REQUIRE(config.isUndirectedFlow());
  CHECK(network.undirectedToDirected()); // adds the two opposite links
  CHECK(network.numLinks() == 4);
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

TEST_CASE("Network rejects explicit multilayer input with regularized flow [fast][core][parser]")
{
  Config config;
  config.silent = true;
  config.regularized = true;
  Network network(config);

  CHECK_THROWS_WITH_AS(
      network.readInputData(infomap::test::repoPath("test/fixtures/networks/multilayer.net")),
      "Regularized multilayer flow requires *Intra/*Inter input; explicit *Multilayer input is not supported with --regularized.",
      std::runtime_error);
}

TEST_CASE("NetworkBuilder validates bipartite intake while building Network state [fast][core][parser]")
{
  const std::string path = "invalid_bipartite_test.net";
  {
    std::ofstream out(path.c_str());
    out << "*Bipartite 4\n";
    out << "1 2 1\n";
  }

  Config config;
  config.silent = true;
  Network network(config);

  CHECK_THROWS_WITH_AS(
      network.readInputData(path),
      "Bipartite link '1 2 1' must cross bipartite start id 4.",
      std::runtime_error);

  std::remove(path.c_str());
}

TEST_CASE("NetworkBuilder owns metadata and multilayer intake summaries [fast][core][parser]")
{
  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(infomap::test::repoPath("test/fixtures/networks/intra_inter.net"));
  CHECK(network.haveMemoryInput());
  CHECK(network.isMultilayerNetwork());

  network.readMetaData(infomap::test::repoPath("test/fixtures/meta/states.meta"));
  CHECK(network.numMetaDataColumns() == 1);
  CHECK(network.metaData().size() == 6);
}

TEST_CASE("NetworkBuilder accumulate=false replaces previous intake state [fast][core][parser]")
{
  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(infomap::test::repoPath("test/fixtures/networks/accumulate_a.net"));
  CHECK(network.nodes().count(1) == 1);
  network.readInputData(infomap::test::repoPath("test/fixtures/networks/accumulate_b.net"), false);

  CHECK(network.nodes().count(1) == 0);
  CHECK(network.nodes().count(3) == 1);
}

TEST_CASE("NetworkInputParser emits events for link-list input [fast][core][parser]")
{
  FakeInputSink sink;

  infomap::input::parseNetworkInput(infomap::test::repoPath("test/fixtures/graphs/twotriangles_unweighted.edges"), sink, defaultInputOptions);

  CHECK_FALSE(sink.fileInput);
  CHECK_FALSE(sink.parsed);
  CHECK(sink.links.size() == 7);
  CHECK(sink.links.front().source == 1);
  CHECK(sink.links.front().target == 2);
  CHECK(sink.links.front().weight == doctest::Approx(1.0));
}

TEST_CASE("NetworkInputParser accepts explicit parser options without querying sink state [fast][core][parser]")
{
  FakeInputSink sink;
  const infomap::input::NetworkInputOptions options { false, 3 };

  infomap::input::parseNetworkInput(infomap::test::repoPath("examples/networks/multilayer.net"), sink, options);

  CHECK(sink.multilayerLinks.size() == 16);
  CHECK(sink.matchableIds == 0);
}

TEST_CASE("NetworkInputParser emits vertex and bipartite events [fast][core][parser]")
{
  FakeInputSink sink;

  infomap::input::parseNetworkInput(infomap::test::repoPath("examples/networks/bipartite.net"), sink, defaultInputOptions);

  CHECK(sink.vertices.size() == 5);
  CHECK(sink.vertices.front().id == 1);
  CHECK(sink.vertices.front().name == "Node 1");
  CHECK(sink.bipartiteStartId == 4);
  CHECK(sink.bipartiteLinks.size() == 4);
}

TEST_CASE("NetworkInputParser emits state-network events [fast][core][parser]")
{
  FakeInputSink sink;

  infomap::input::parseNetworkInput(infomap::test::repoPath("examples/networks/states.net"), sink, defaultInputOptions);

  CHECK(sink.vertices.size() == 5);
  CHECK(sink.stateNodes.size() == 6);
  CHECK(sink.stateNodes.front().node.id == 1);
  CHECK(sink.stateNodes.front().node.physicalId == 1);
  CHECK(sink.links.size() == 16);
}

TEST_CASE("NetworkInputParser emits explicit multilayer events [fast][core][parser]")
{
  FakeInputSink sink;

  infomap::input::parseNetworkInput(infomap::test::repoPath("examples/networks/multilayer.net"), sink, defaultInputOptions);

  CHECK(sink.vertices.size() == 5);
  CHECK(sink.multilayerLinks.size() == 16);
  CHECK(sink.numIntraLayerLinks() == 12);
  CHECK(sink.numInterLayerLinks() == 4);
}

TEST_CASE("NetworkInputParser emits intra and inter events [fast][core][parser]")
{
  FakeInputSink sink;

  infomap::input::parseNetworkInput(infomap::test::repoPath("test/fixtures/networks/intra_inter.net"), sink, defaultInputOptions);

  CHECK(sink.intraLinks.size() == 2);
  CHECK(sink.interLinks.size() == 2);
  CHECK(sink.intraLinks.front().layer == 1);
  CHECK(sink.interLinks.front().sourceLayer == 1);
  CHECK(sink.interLinks.front().targetLayer == 2);
}

TEST_CASE("NetworkInputParser emits metadata events [fast][core][parser]")
{
  FakeInputSink sink;

  infomap::input::parseMetaDataInput(infomap::test::repoPath("test/fixtures/meta/states.meta"), sink);

  CHECK(sink.metaDataRows.size() == 6);
  CHECK(sink.metaDataRows.front().size() == 1);
}

TEST_CASE("NetworkInputParser preserves malformed multilayer errors [fast][core][parser]")
{
  FakeInputSink sink;

  CHECK_THROWS_WITH_AS(
      infomap::input::parseNetworkInput(infomap::test::repoPath("test/fixtures/networks/invalid_multilayer.net"), sink, defaultInputOptions),
      "Can't parse multilayer link data from line '1 1 broken'",
      std::runtime_error);
}

TEST_CASE("StateNetwork builds CSR link storage after finalize [fast][core][csr]")
{
  Network network(Config{});
  network.addLink(1, 2, 1.0);
  network.addLink(1, 3, 2.0);
  network.addLink(2, 3, 4.0);
  network.finalizeLinks();

  CHECK(network.numNodes() == 3);
  CHECK(network.numLinks() == 3);
  CHECK(network.outDegree(network.indexOfId(1)) == 2); // node 1 -> {2,3}
  CHECK(network.outDegree(network.indexOfId(2)) == 1); // node 2 -> {3}
  CHECK(network.isDangling(network.indexOfId(3)));      // node 3 has no out-links
  CHECK(network.nodeId(0) == 1);                         // sorted ids {1,2,3}

  std::vector<std::tuple<unsigned int, unsigned int, double>> seen;
  network.forEachLink([&](unsigned int s, unsigned int t, double w, double&) {
    seen.emplace_back(network.nodeId(s), network.nodeId(t), w);
  });
  REQUIRE(seen.size() == 3);
  CHECK(std::get<0>(seen[0]) == 1);
  CHECK(std::get<1>(seen[0]) == 2);
  CHECK(std::get<2>(seen[2]) == doctest::Approx(4.0));
}

} // namespace
