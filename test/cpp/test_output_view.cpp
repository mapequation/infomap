#include "vendor/doctest.h"

#include "Infomap.h"
#include "io/Output.h"
#include "io/OutputView.h"

#include "TestUtils.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using infomap::InfomapWrapper;

struct ProjectedLeaf {
  unsigned int stateId = 0;
  unsigned int physicalId = 0;
  unsigned int layerId = 0;
  unsigned int moduleId = 0;
  double flow = 0.0;
  std::string name;
};

std::vector<ProjectedLeaf> leafRows(InfomapWrapper& im, bool states)
{
  infomap::OutputView view(im, im.network(), states);
  std::vector<ProjectedLeaf> rows;
  view.forEachLeaf(1, infomap::OutputLeafPolicy::HideBipartite, [&](const infomap::OutputLeafRow& row) {
    rows.push_back({ row.stateId, row.physicalId, row.layerId, row.moduleId, row.flow, row.name });
  });
  std::sort(rows.begin(), rows.end(), [](const ProjectedLeaf& lhs, const ProjectedLeaf& rhs) {
    return std::make_pair(lhs.stateId, lhs.physicalId) < std::make_pair(rhs.stateId, rhs.physicalId);
  });
  return rows;
}

std::unique_ptr<InfomapWrapper> runTwoTriangles()
{
  return infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::addEdgeFixtureLinks(infomap, "graphs/twotriangles_unweighted.edges"); });
}

std::string outputPath(const std::string& name)
{
  return "output_view_" + name;
}

void removeOutput(const std::string& path)
{
  std::remove(path.c_str());
}

} // namespace

TEST_CASE("OutputView projects first-order leaf rows [fast][core][output]")
{
  auto im = runTwoTriangles();

  const auto rows = leafRows(*im, false);

  REQUIRE(rows.size() == 6);
  for (const auto& row : rows) {
    CHECK(row.stateId == row.physicalId);
    CHECK(row.moduleId > 0);
    CHECK(row.flow > 0.0);
    CHECK(row.name == std::to_string(row.physicalId));
  }
}

TEST_CASE("OutputView projects state leaf rows separately from physical ids [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::readNetworkFixture(infomap, "states.net"); });

  const auto physicalRows = leafRows(*im, false);
  const auto stateRows = leafRows(*im, true);

  std::set<unsigned int> physicalIdsFromStates;
  for (const auto& row : stateRows) {
    physicalIdsFromStates.insert(row.physicalId);
  }

  CHECK(physicalRows.size() <= stateRows.size());
  CHECK(stateRows.size() == 6);
  CHECK(physicalIdsFromStates.size() == 5);
}

TEST_CASE("OutputView preserves multilayer layer ids in state projection [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap::test::readNetworkFixture(infomap, "multilayer.net"); });

  infomap::OutputView view(*im, im->network(), true);
  bool foundLayer = false;
  view.forEachLeaf(1, infomap::OutputLeafPolicy::HideBipartite, [&](const infomap::OutputLeafRow& row) {
    if (row.layerId != 0) {
      foundLayer = true;
    }
  });

  CHECK(view.isMultilayer());
  CHECK(foundLayer);
}

TEST_CASE("OutputView applies bipartite leaf filtering [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap.readInputData(infomap::test::repoPath("examples/networks/bipartite.net")); });
  im->hideBipartiteNodes = true;

  infomap::OutputView view(*im, im->network(), false);
  std::vector<unsigned int> physicalIds;
  view.forEachLeaf(1, infomap::OutputLeafPolicy::HideBipartite, [&](const infomap::OutputLeafRow& row) {
    physicalIds.push_back(row.physicalId);
  });

  std::sort(physicalIds.begin(), physicalIds.end());
  CHECK(physicalIds == std::vector<unsigned int> { 1, 2, 3 });
}

TEST_CASE("The tree header records how many trials produced it [fast][core][output]")
{
  // The header recorded the requested --num-trials and an elapsed time but never the
  // trials that actually ran, so the artifact an interrupted run leaves behind was
  // indistinguishable from a complete search (#906).
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap.readInputData(infomap::test::repoPath("examples/networks/twotriangles.net")); },
      "--num-trials 3");

  const std::string treePath = "trial_count_header.tree";
  std::remove(treePath.c_str());
  infomap::writeTree(*im, im->network(), treePath, false);

  const auto tree = infomap::test::readTextFile(treePath);
  CHECK(tree.find("# trials 3 of 3") != std::string::npos);

  std::remove(treePath.c_str());
}

TEST_CASE("A node name with a quote survives the tree and csv writers [fast][core][output][parser]")
{
  // The writers emit the name between quotes with no escaping. The csv row then had one
  // more column than its header, and Infomap could not read back its own .tree at all --
  // "Couldn't parse node id" -- because the parser read up to the *first* quote (#908).
  auto im = infomap::test::makeRunningInfomap([&](InfomapWrapper& infomap) {
    infomap.readInputData(infomap::test::networkFixturePath("quoted_name.net"));
  });

  const std::string treePath = "quoted_name_roundtrip.tree";
  const std::string csvPath = "quoted_name_roundtrip.csv";
  std::remove(treePath.c_str());
  std::remove(csvPath.c_str());

  infomap::writeTree(*im, im->network(), treePath, false);
  infomap::writeCsvTree(*im, im->network(), csvPath, false);

  const auto tree = infomap::test::readTextFile(treePath);
  const auto csv = infomap::test::readTextFile(csvPath);

  // The tree keeps the name verbatim; the csv doubles the quote, per RFC 4180.
  CHECK(tree.find("\"gene \"X\", alias\"") != std::string::npos);
  CHECK(csv.find("\"gene \"\"X\"\", alias\"") != std::string::npos);

  // Every csv row has the same number of fields as the header.
  std::istringstream csvStream(csv);
  std::string headerLine;
  REQUIRE(std::getline(csvStream, headerLine));
  const auto countFields = [](const std::string& line) {
    std::size_t fields = 1;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
      if (line[i] == '"') {
        const bool doubled = i + 1 < line.size() && line[i + 1] == '"';
        if (doubled)
          ++i;
        else
          inQuotes = !inQuotes;
      } else if (line[i] == ',' && !inQuotes) {
        ++fields;
      }
    }
    return fields;
  };
  const auto headerFields = countFields(headerLine);
  std::string row;
  unsigned int rows = 0;
  while (std::getline(csvStream, row)) {
    if (row.empty())
      continue;
    ++rows;
    CHECK(countFields(row) == headerFields);
  }
  CHECK(rows == 3);

  // And the tree reads back through the documented path: the parser takes the name from
  // the first quote to the last, the same rule the network parser uses.
  InfomapWrapper reader(infomap::test::defaultFlags("--no-infomap --cluster-data " + treePath));
  reader.readInputData(infomap::test::networkFixturePath("quoted_name.net"));
  CHECK_NOTHROW(reader.run());
  // Not checkRunSanity: this partition's codelength is exactly zero, which comes out as
  // a negative epsilon that the helper rejects. What matters here is that the file was
  // read at all and every node came back.
  CHECK(reader.numLeafNodes() == 3);

  std::remove(treePath.c_str());
  std::remove(csvPath.c_str());
}

TEST_CASE("Bipartite hiding is decided per output, not by the flow-tree flag [fast][core][output]")
{
  auto im = infomap::test::makeRunningInfomap(
      [&](InfomapWrapper& infomap) { infomap.readInputData(infomap::test::repoPath("examples/networks/bipartite.net")); });
  im->hideBipartiteNodes = true;
  // Asking for the flow tree used to un-hide the feature nodes in the plain .tree as
  // well: one writer serves both files, so it consulted this global flag instead of
  // being told which file it was writing (#908).
  im->printFlowTree = true;

  infomap::OutputView view(*im, im->network(), false);
  auto physicalIdsWith = [&](infomap::OutputLeafPolicy policy) {
    std::vector<unsigned int> ids;
    view.forEachLeaf(1, policy, [&](const infomap::OutputLeafRow& row) { ids.push_back(row.physicalId); });
    std::sort(ids.begin(), ids.end());
    return ids;
  };

  CHECK(physicalIdsWith(infomap::OutputLeafPolicy::HideBipartite) == std::vector<unsigned int> { 1, 2, 3 });
  // The flow tree keeps them, because its link section refers to them.
  CHECK(physicalIdsWith(infomap::OutputLeafPolicy::KeepBipartite) == std::vector<unsigned int> { 1, 2, 3, 4, 5 });
}

TEST_CASE("OutputView owns module-link projection [fast][core][output]")
{
  auto im = runTwoTriangles();
  infomap::OutputView view(*im, im->network(), false);

  const auto moduleLinks = view.moduleLinks();

  REQUIRE(!moduleLinks.empty());
  bool foundPositiveFlow = false;
  for (const auto& moduleEntry : moduleLinks) {
    for (const auto& linkEntry : moduleEntry.second) {
      CHECK(linkEntry.first.first != linkEntry.first.second);
      CHECK(linkEntry.second >= 0.0);
      foundPositiveFlow = foundPositiveFlow || linkEntry.second > 0.0;
    }
  }
  CHECK(foundPositiveFlow);
}

TEST_CASE("OutputView exposes serializable module rows for output adapters [fast][core][output]")
{
  auto im = runTwoTriangles();
  infomap::OutputView view(*im, im->network(), false);

  unsigned int numModules = 0;
  bool foundRoot = false;
  bool foundLinks = false;
  view.forEachModule([&](const infomap::OutputModuleRow& module) {
    ++numModules;
    if (module.linkPathLabel == "root") {
      foundRoot = true;
      CHECK(module.jsonPath.empty());
      CHECK(module.numChildren > 0);
      CHECK(module.codelength >= 0.0);
    }
    foundLinks = foundLinks || !module.links.empty();
  });

  CHECK(numModules > 0);
  CHECK(foundRoot);
  CHECK(foundLinks);
}

TEST_CASE("OutputView refactor preserves stable tree output fields [fast][core][output]")
{
  auto im = runTwoTriangles();
  const auto treePath = outputPath("tree.tree");
  removeOutput(treePath);

  im->writeTree(treePath);
  const auto treeText = infomap::test::readTextFile(treePath);

  CHECK(treeText.find("# path flow name node_id") != std::string::npos);
  CHECK(treeText.find("\"1\" 1") != std::string::npos);
  CHECK(treeText.find("\"6\" 6") != std::string::npos);

  removeOutput(treePath);
}

TEST_CASE("OutputView refactor preserves stable JSON output fields [fast][core][output]")
{
  auto im = runTwoTriangles();
  const auto jsonPath = outputPath("tree.json");
  removeOutput(jsonPath);

  im->writeJsonTree(jsonPath);
  const auto jsonText = infomap::test::readTextFile(jsonPath);

  CHECK(jsonText.find("\"nodes\":[") != std::string::npos);
  CHECK(jsonText.find("\"modules\":[") != std::string::npos);
  CHECK(jsonText.find("\"directed\":false") != std::string::npos);
  CHECK(jsonText.find("\"flowModel\":\"undirected\"") != std::string::npos);
  CHECK(jsonText.find("\"flow\":0.142857") != std::string::npos);
  CHECK(jsonText.find("0.142857142857") == std::string::npos);

  removeOutput(jsonPath);
}
