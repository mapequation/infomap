#include "vendor/doctest.h"

#include "io/Config.h"
#include "io/Network.h"
#include "io/NetworkInputParser.h"
#include "io/JsonNetworkInputParser.h"

#include "TestUtils.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
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

  struct InitialPath {
    unsigned int id;
    std::vector<unsigned int> path;
    bool stateLevel;
  };
  std::vector<InitialPath> initialPaths;

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
  void onInitialPartitionPath(unsigned int id, const std::vector<unsigned int>& path, bool stateLevel) { initialPaths.push_back({ id, path, stateLevel }); }

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

TEST_CASE("addLink rejects ill-defined weights [fast][core]")
{
  Config config;
  config.silent = true;
  Network network(config);

  // Negative, NaN and infinite weights are never valid for the map equation
  // (weights feed a flow distribution), so ingestion must reject them rather
  // than silently computing a meaningless result.
  CHECK_THROWS_AS(network.addLink(0, 1, -1.0), std::invalid_argument);
  CHECK_THROWS_AS(network.addLink(0, 1, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
  CHECK_THROWS_AS(network.addLink(0, 1, std::numeric_limits<double>::infinity()), std::invalid_argument);
  CHECK_THROWS_AS(network.addLink(0, 1, -std::numeric_limits<double>::infinity()), std::invalid_argument);

  // No link survived rejection.
  CHECK(network.numLinks() == 0);

  // Zero-weight links are well-defined (no flow) and stay allowed -- they are
  // silently filtered by the weight threshold, not rejected.
  CHECK_FALSE(network.addLink(0, 1, 0.0));
  CHECK(network.numLinks() == 0);

  // A finite, positive weight is accepted as before.
  CHECK(network.addLink(0, 1, 2.0));
  network.finalizeLinks();
  CHECK(network.numLinks() == 1);
}

TEST_CASE("Mode-A map APIs work: outWeights / removeLink [fast][core][csr]")
{
  Config config;
  config.silent = true;
  Network network(config);
  network.addLink(1, 2, 1.0);
  network.addLink(1, 3, 2.0);
  network.addLink(2, 3, 4.0);

  // outWeights() is derived on demand for the flat-buffer (mode-A) build, and
  // must finalize lazily -- no explicit finalizeLinks() call here.
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

TEST_CASE("addLink after finalize re-merges via definalize, preserving FP order [fast][core][csr]")
{
  Config config;
  config.silent = true;
  Network network(config);
  network.addLink(1, 2, 0.1);
  network.addLink(1, 2, 0.2);
  network.finalizeLinks(); // (1,2) merged to (0.1 + 0.2)
  CHECK(network.numLinks() == 1);

  // addLink after finalize -> definalize() restores the buffer, appends, and a
  // later finalize re-sorts + re-merges (accumulate / addLink-after-run path).
  network.addLink(1, 2, 0.3);
  network.addLink(1, 3, 4.0);
  network.finalizeLinks();

  CHECK(network.numLinks() == 2);
  double w12 = -1.0, w13 = -1.0;
  network.forEachLink([&](unsigned int s, unsigned int t, double w, double&) {
    if (network.nodeId(s) == 1 && network.nodeId(t) == 2) w12 = w;
    if (network.nodeId(s) == 1 && network.nodeId(t) == 3) w13 = w;
  });
  // The merged (0.1 + 0.2) is preserved as one occurrence, then + 0.3 in arrival
  // order -- bit-identical to merging all three occurrences in one pass.
  CHECK(w12 == ((0.1 + 0.2) + 0.3));
  CHECK(w13 == doctest::Approx(4.0));
}

TEST_CASE("clearLinks before finalize leaves no garbage counts [fast][core][csr]")
{
  Config config;
  config.silent = true;
  Network network(config);
  network.addLink(1, 2, 1.0);
  network.addLink(1, 2, 2.0); // duplicate: would aggregate on finalize
  network.clearLinks();       // cleared while still unfinalized (mode A)

  // A lazy finalize here must NOT rebuild from cleared buffers and compute
  // m_numAggregatedLinks = m_rawLinkCount - 0.
  CHECK(network.numLinks() == 0);
  CHECK(network.numAggregatedLinks() == 0);
}

TEST_CASE("Isolated/dangling node is in-bounds in mode-A CSR [fast][core][csr]")
{
  Config config;
  config.silent = true;
  Network network(config);
  network.addLink(1, 2, 1.0);
  network.addNode(9); // isolated node with the largest id -> last CSR index
  network.finalizeLinks();

  CHECK(network.numNodes() == 3);
  CHECK(network.numLinks() == 1);
  // The largest-id node sits at the last CSR index; outDegree()/isDangling()
  // read m_linkOffsets[index+1] and must stay in bounds.
  CHECK(network.isDangling(network.indexOfId(9)));
  CHECK(network.outDegree(network.indexOfId(9)) == 0);
  CHECK_FALSE(network.isDangling(network.indexOfId(1)));
  unsigned int emitted = 0;
  network.forEachLink([&](unsigned int, unsigned int, double, double&) { ++emitted; });
  CHECK(emitted == 1); // the isolated node contributes no link
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

TEST_CASE("multilayer-relax-to-self couples diagonal nodes in the simulated path [fast][core][multilayer]")
{
  // Directed so out-links are exactly as added (no undirected->directed expansion).
  Network network(Config("--silent --directed --multilayer-relax-to-self", false));
  // Layer 1: node 1 -> 2
  network.addMultilayerIntraLink(1, 1, 2, 1.0);
  network.addMultilayerIntraLink(1, 2, 1, 1.0);
  // Layer 2: node 1 has three out-neighbours
  network.addMultilayerIntraLink(2, 1, 2, 1.0);
  network.addMultilayerIntraLink(2, 1, 3, 1.0);
  network.addMultilayerIntraLink(2, 1, 4, 1.0);

  network.postProcessInputData();

  const auto& map = network.layerNodeToStateId();
  auto sid = [&](unsigned layer, unsigned phys) { return map.at(layer).at(phys); };
  const unsigned s11 = sid(1, 1);

  std::map<unsigned int, double> out; // target state id -> weight
  network.forEachLink([&](unsigned s, unsigned t, double w, double) {
    if (network.nodeId(s) == s11) out[network.nodeId(t)] = w;
  });

  // Home layer unchanged from spread; one diagonal inter link carrying the
  // spread per-layer aggregate.
  CHECK(out.size() == 2);
  CHECK(out.count(sid(1, 2)) == 1);
  CHECK(out.at(sid(1, 2)) == doctest::Approx(0.8875)); // home = spread: 0.15/4 + 0.85/1
  CHECK(out.count(sid(2, 1)) == 1);
  CHECK(out.at(sid(2, 1)) == doctest::Approx(0.1125)); // diagonal aggregate: 0.15 * 3/4
  // No inter links to neighbours in the target layer.
  CHECK(out.count(sid(2, 2)) == 0);
  CHECK(out.count(sid(2, 3)) == 0);
  CHECK(out.count(sid(2, 4)) == 0);
}

TEST_CASE("default simulated path still spreads to neighbours [fast][core][multilayer]")
{
  Network network(Config("--silent --directed", false));
  network.addMultilayerIntraLink(1, 1, 2, 1.0);
  network.addMultilayerIntraLink(1, 2, 1, 1.0);
  network.addMultilayerIntraLink(2, 1, 2, 1.0);
  network.addMultilayerIntraLink(2, 1, 3, 1.0);
  network.addMultilayerIntraLink(2, 1, 4, 1.0);

  network.postProcessInputData();

  const auto& map = network.layerNodeToStateId();
  auto sid = [&](unsigned layer, unsigned phys) { return map.at(layer).at(phys); };
  const unsigned s11 = sid(1, 1);

  std::map<unsigned int, double> out;
  network.forEachLink([&](unsigned s, unsigned t, double w, double) {
    if (network.nodeId(s) == s11) out[network.nodeId(t)] = w;
  });

  // Home intra link + three spread inter links to the target-layer neighbours.
  CHECK(out.size() == 4);
  CHECK(out.count(sid(2, 1)) == 0); // no diagonal coupling
  CHECK(out.count(sid(2, 2)) == 1);
  CHECK(out.count(sid(2, 3)) == 1);
  CHECK(out.count(sid(2, 4)) == 1);
}

TEST_CASE("multilayer-relax-to-self couples diagonal nodes in the explicit *Inter path [fast][core][multilayer]")
{
  Network network(Config("--silent --directed --multilayer-relax-to-self", false));
  // Intra
  network.addMultilayerIntraLink(1, 1, 2, 1.0);
  network.addMultilayerIntraLink(2, 1, 2, 1.0);
  network.addMultilayerIntraLink(2, 1, 3, 1.0);
  // Explicit inter link from (layer1, node1) to layer2
  network.addMultilayerInterLink(1, 1, 2, 0.5);

  network.postProcessInputData();

  const auto& map = network.layerNodeToStateId();
  auto sid = [&](unsigned layer, unsigned phys) { return map.at(layer).at(phys); };
  const unsigned s11 = sid(1, 1);

  std::map<unsigned int, double> out;
  network.forEachLink([&](unsigned s, unsigned t, double w, double) {
    if (network.nodeId(s) == s11) out[network.nodeId(t)] = w;
  });

  CHECK(out.count(sid(1, 2)) == 1); // intra, full weight
  CHECK(out.at(sid(1, 2)) == doctest::Approx(1.0));
  CHECK(out.count(sid(2, 1)) == 1); // single diagonal inter link
  CHECK(out.at(sid(2, 1)) == doctest::Approx(0.5)); // == interWeight
  CHECK(out.count(sid(2, 2)) == 0); // not spread to neighbours
  CHECK(out.count(sid(2, 3)) == 0);
}

TEST_CASE("explicit *Inter to-self links even when the node is absent in the target layer [fast][core][multilayer]")
{
  Network network(Config("--silent --directed --multilayer-relax-to-self", false));
  network.addMultilayerIntraLink(1, 1, 2, 1.0);
  // Layer 2 exists but node 1 has NO intra out-links there.
  network.addMultilayerIntraLink(2, 5, 6, 1.0);
  network.addMultilayerInterLink(1, 1, 2, 0.5);

  network.postProcessInputData();

  const auto& map = network.layerNodeToStateId();
  auto sid = [&](unsigned layer, unsigned phys) { return map.at(layer).at(phys); };
  const unsigned s11 = sid(1, 1);

  std::map<unsigned int, double> out;
  network.forEachLink([&](unsigned s, unsigned t, double w, double) {
    if (network.nodeId(s) == s11) out[network.nodeId(t)] = w;
  });

  // The explicit coupling is still created even though (2,1) is dangling.
  CHECK(out.count(sid(2, 1)) == 1);
  CHECK(out.at(sid(2, 1)) == doctest::Approx(0.5));
}

TEST_CASE("multilayer-relax-to-self adds reverse diagonal inter link for undirected flow [fast][core][multilayer]")
{
  Network network(Config("--silent --multilayer-relax-to-self", false)); // undirected (default)
  network.addMultilayerIntraLink(1, 1, 2, 1.0);
  network.addMultilayerIntraLink(2, 1, 2, 1.0);
  network.addMultilayerInterLink(1, 1, 2, 0.5);

  network.postProcessInputData();

  const auto& map = network.layerNodeToStateId();
  auto sid = [&](unsigned layer, unsigned phys) { return map.at(layer).at(phys); };

  std::map<unsigned int, std::map<unsigned int, double>> out; // src -> (tgt -> w)
  network.forEachLink([&](unsigned s, unsigned t, double w, double) {
    out[network.nodeId(s)][network.nodeId(t)] = w;
  });

  // Forward and reverse diagonal coupling, both with interWeight.
  CHECK(out[sid(1, 1)].count(sid(2, 1)) == 1);
  CHECK(out[sid(1, 1)].at(sid(2, 1)) == doctest::Approx(0.5));
  CHECK(out[sid(2, 1)].count(sid(1, 1)) == 1);
  CHECK(out[sid(2, 1)].at(sid(1, 1)) == doctest::Approx(0.5));
  // Never spread to neighbours.
  CHECK(out[sid(1, 1)].count(sid(2, 2)) == 0);
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

TEST_CASE("looksLikeJsonNetwork detects JSON by content [fast][core][parser][json]")
{
  CHECK(infomap::input::looksLikeJsonNetwork(infomap::test::repoPath("test/fixtures/networks/json/standard_minimal.json")));
  CHECK_FALSE(infomap::input::looksLikeJsonNetwork(infomap::test::repoPath("test/fixtures/networks/states.net")));
}

TEST_CASE("JSON parser emits link events for a standard network [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/standard_minimal.json"), sink, defaultInputOptions);

  CHECK(sink.vertices.empty());
  CHECK(sink.links.size() == 3);
  CHECK(sink.links.front().source == 1);
  CHECK(sink.links.front().target == 2);
  CHECK(sink.links.front().weight == doctest::Approx(1.0));
}

TEST_CASE("JSON parser emits vertex events with names and weights [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/standard.json"), sink, defaultInputOptions);

  CHECK(sink.vertices.size() == 2);
  CHECK(sink.vertices.front().id == 1);
  CHECK(sink.vertices.front().name == "Species A");
  CHECK(sink.vertices.front().hasWeight);
  CHECK(sink.vertices.front().weight == doctest::Approx(2.0));
  CHECK(sink.vertices.back().id == 2);
  CHECK_FALSE(sink.vertices.back().hasWeight);
  CHECK(sink.links.size() == 1);
}

TEST_CASE("JSON parser is emitter-independent (edges before discriminators) [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/standard_keys_sorted.json"), sink, defaultInputOptions);

  CHECK(sink.links.size() == 1);
  CHECK(sink.links.front().source == 1);
  CHECK(sink.links.front().target == 2);
}

TEST_CASE("JSON parser accepts integral-valued doubles as ids [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/coercion_integral_double.json"), sink, defaultInputOptions);

  CHECK(sink.vertices.size() == 1);
  CHECK(sink.vertices.front().id == 10);
  CHECK(sink.links.size() == 1);
  CHECK(sink.links.front().source == 10);
  CHECK(sink.links.front().target == 11);
}

TEST_CASE("JSON parser rejects non-integral ids [fast][core][parser][json]")
{
  FakeInputSink sink;

  CHECK_THROWS_AS(
      infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/invalid/noninteger_id.json"), sink, defaultInputOptions),
      std::runtime_error);
}

TEST_CASE("Network builds a standard network from JSON via format detection [fast][core][parser][json]")
{
  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(infomap::test::repoPath("test/fixtures/networks/json/standard_minimal.json"));

  CHECK_FALSE(network.haveMemoryInput());
  CHECK(network.numNodes() == 3);
  CHECK(network.numLinks() == 3);
}

TEST_CASE("JSON parser tolerates unknown fields [fast][core][parser][json]")
{
  const std::string path = "json_unknown_field_test.json";
  {
    std::ofstream out(path.c_str());
    out << R"({"format":"infomap-network","version":"1.0",)"
        << R"("description":"a comment","customRootAttr":42,)"
        << R"("edges":[{"source":1,"target":2,"wieght":5.0}]})";
  }

  FakeInputSink sink;
  infomap::input::parseJsonNetworkInput(path, sink, defaultInputOptions);

  CHECK(sink.links.size() == 1);
  CHECK(sink.links.front().weight == doctest::Approx(1.0)); // typo'd weight ignored, default used

  std::remove(path.c_str());
}

TEST_CASE("JSON parser emits bipartite events [fast][core][parser][json]")
{
  FakeInputSink sink;
  sink.onBipartiteStart(0);

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/bipartite.json"), sink, defaultInputOptions);

  CHECK(sink.bipartiteStartId == 4);
  CHECK(sink.bipartiteLinks.size() == 1);
  CHECK(sink.bipartiteLinks.front().source == 1);
  CHECK(sink.bipartiteLinks.front().target == 4);
  CHECK(sink.vertices.size() == 2);
}

TEST_CASE("JSON parser rejects bipartite without start id [fast][core][parser][json]")
{
  FakeInputSink sink;

  CHECK_THROWS_AS(
      infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/invalid/bipartite_no_start.json"), sink, defaultInputOptions),
      std::runtime_error);
}

TEST_CASE("JSON parser emits explicit state events [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/state.json"), sink, defaultInputOptions);

  CHECK(sink.vertices.size() == 2);
  CHECK(sink.stateNodes.size() == 2);
  CHECK(sink.stateNodes.front().node.id == 1);
  CHECK(sink.stateNodes.front().node.physicalId == 1);
  CHECK(sink.stateNodes.front().hasWeight);
  CHECK(sink.stateNodes.front().node.weight == doctest::Approx(0.4));
  CHECK(sink.links.size() == 1);
}

TEST_CASE("JSON parser infers identity states when states[] omitted [fast][core][parser][json]")
{
  const std::string path = "json_state_inferred_test.json";
  {
    std::ofstream out(path.c_str());
    out << R"({"format":"infomap-network","version":"1.0","type":"state",)"
        << R"("edges":[{"source":1,"target":2},{"source":2,"target":3}]})";
  }

  FakeInputSink sink;
  infomap::input::parseJsonNetworkInput(path, sink, defaultInputOptions);

  CHECK(sink.stateNodes.size() == 3); // 1, 2, 3 inferred once each
  CHECK(sink.stateNodes.front().node.id == sink.stateNodes.front().node.physicalId);
  CHECK(sink.links.size() == 2);

  std::remove(path.c_str());
}

TEST_CASE("JSON parser emits full multilayer events [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/multilayer_full.json"), sink, defaultInputOptions);

  REQUIRE(sink.multilayerLinks.size() == 1);
  CHECK(sink.multilayerLinks.front().sourceLayer == 1);
  CHECK(sink.multilayerLinks.front().sourceNode == 1);
  CHECK(sink.multilayerLinks.front().targetLayer == 2);
  CHECK(sink.multilayerLinks.front().targetNode == 2);
}

TEST_CASE("JSON parser emits intra-layer multilayer events [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/multilayer_intra.json"), sink, defaultInputOptions);

  REQUIRE(sink.intraLinks.size() == 1);
  CHECK(sink.intraLinks.front().layer == 1);
  CHECK(sink.intraLinks.front().sourceNode == 1);
  CHECK(sink.intraLinks.front().targetNode == 2);
}

TEST_CASE("JSON parser emits intra-inter multilayer events [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/multilayer_intra_inter.json"), sink, defaultInputOptions);

  REQUIRE(sink.intraLinks.size() == 1);
  REQUIRE(sink.interLinks.size() == 1);
  CHECK(sink.intraLinks.front().layer == 1);
  CHECK(sink.interLinks.front().sourceLayer == 1);
  CHECK(sink.interLinks.front().node == 1);
  CHECK(sink.interLinks.front().targetLayer == 2);
}

TEST_CASE("JSON parser rejects full multilayer edge with one layer [fast][core][parser][json]")
{
  FakeInputSink sink;

  CHECK_THROWS_AS(
      infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/invalid/multilayer_full_one_layer.json"), sink, defaultInputOptions),
      std::runtime_error);
}

TEST_CASE("JSON parser rejects intra-inter inter-layer edge with source != target [fast][core][parser][json]")
{
  const std::string path = "json_intra_inter_bad_test.json";
  {
    std::ofstream out(path.c_str());
    out << R"({"format":"infomap-network","version":"1.0","type":"multilayer","multilayer":"intra-inter",)"
        << R"("edges":[{"layers":[1,2],"source":1,"target":2}]})";
  }

  FakeInputSink sink;
  CHECK_THROWS_AS(infomap::input::parseJsonNetworkInput(path, sink, defaultInputOptions), std::runtime_error);

  std::remove(path.c_str());
}

TEST_CASE("Network builds a higher-order state network from JSON [fast][core][parser][json]")
{
  // Two states (1, 2) on the same physical node 1 makes this genuinely
  // higher-order; the RFC state.json example uses an identity mapping that the
  // core (correctly) does not treat as memory input.
  const std::string path = "json_state_higher_order_test.json";
  {
    std::ofstream out(path.c_str());
    out << R"({"format":"infomap-network","version":"1.0","type":"state",)"
        << R"("states":[{"id":1,"node":1},{"id":2,"node":1},{"id":3,"node":2}],)"
        << R"("edges":[{"source":1,"target":3},{"source":2,"target":3}]})";
  }

  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(path);

  CHECK(network.haveMemoryInput());
  CHECK(network.numNodes() == 3);
  CHECK(network.numPhysicalNodes() == 2);

  std::remove(path.c_str());
}

TEST_CASE("Network builds a multilayer network from JSON [fast][core][parser][json]")
{
  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(infomap::test::repoPath("test/fixtures/networks/json/multilayer_intra.json"));

  CHECK(network.haveMemoryInput());
  CHECK(network.isMultilayerNetwork());
}

TEST_CASE("JSON parser emits embedded meta events [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/standard.json"), sink, defaultInputOptions);

  REQUIRE(sink.metaDataRows.size() == 2);
  CHECK(sink.metaDataRows.front().size() == 1);
  CHECK(sink.metaDataRows.front().front() == 10);
  CHECK(sink.metaDataRows.back().front() == 20);
}

TEST_CASE("Network loads embedded meta from JSON (presence enables, like set_meta_data) [fast][core][parser][json]")
{
  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(infomap::test::repoPath("test/fixtures/networks/json/standard.json"));

  CHECK(network.numMetaDataColumns() == 1);
  CHECK(network.metaData().size() == 2);
  CHECK(network.metaData().at(1).front() == 10);
  CHECK(network.metaData().at(2).front() == 20);
}

TEST_CASE("JSON parser emits node initial-partition paths [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/standard.json"), sink, defaultInputOptions);

  REQUIRE(sink.initialPaths.size() == 2);
  CHECK(sink.initialPaths.front().id == 1);
  CHECK(sink.initialPaths.front().path == std::vector<unsigned int> { 1 });
  CHECK_FALSE(sink.initialPaths.front().stateLevel);
  CHECK(sink.initialPaths.back().path == std::vector<unsigned int> { 1, 2 });
}

TEST_CASE("JSON parser emits state-level initial-partition paths [fast][core][parser][json]")
{
  FakeInputSink sink;

  infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/state.json"), sink, defaultInputOptions);

  REQUIRE(sink.initialPaths.size() == 2);
  CHECK(sink.initialPaths.front().stateLevel);
  CHECK(sink.initialPaths.front().path == std::vector<unsigned int> { 1 });
  CHECK(sink.initialPaths.back().path == std::vector<unsigned int> { 1, 2 });
}

TEST_CASE("JSON parser rejects path on a multilayer node [fast][core][parser][json]")
{
  FakeInputSink sink;

  CHECK_THROWS_AS(
      infomap::input::parseJsonNetworkInput(infomap::test::repoPath("test/fixtures/networks/json/invalid/multilayer_node_path.json"), sink, defaultInputOptions),
      std::runtime_error);
}

TEST_CASE("Network retains embedded initial-partition paths from JSON [fast][core][parser][json]")
{
  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(infomap::test::repoPath("test/fixtures/networks/json/twotriangles_paths.json"));

  CHECK(network.initialPartitionPaths().size() == 6);
}

TEST_CASE("External --meta-data composes with a JSON network [fast][core][parser][json]")
{
  const std::string meta = "json_external_meta_test.meta";
  {
    std::ofstream out(meta.c_str());
    out << "1 5\n2 5\n3 7\n";
  }

  Config config;
  config.silent = true;
  Network network(config);

  network.readInputData(infomap::test::repoPath("test/fixtures/networks/json/standard_minimal.json"));
  network.readMetaData(meta);

  CHECK(network.numMetaDataColumns() == 1);
  CHECK(network.metaData().size() == 3);
  CHECK(network.metaData().at(3).front() == 7);

  std::remove(meta.c_str());
}

} // namespace
