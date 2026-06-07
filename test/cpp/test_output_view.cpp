#include "vendor/doctest.h"

#include "Infomap.h"
#include "io/OutputView.h"

#include "TestUtils.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <set>
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

  removeOutput(jsonPath);
}
