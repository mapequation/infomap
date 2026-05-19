#include "vendor/doctest.h"

#include "io/Config.h"
#include "io/OutputFormats.h"

#include <map>
#include <set>

namespace {

using infomap::Config;
TEST_CASE("Config accepts no-file-output without an output directory [fast][core][config][cli]")
{
  CHECK_NOTHROW(Config("input.net --silent --no-file-output", true));

  const Config config("input.net --silent --no-file-output", true);
  CHECK(config.noFileOutput);
  CHECK(config.outDirectory.empty());
  CHECK(config.networkFile == "input.net");
}

TEST_CASE("Config parses flow model selection and output formats [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --flow-model precomputed --output json,tree", true);
  CHECK(infomap::flowModelToString(config.flowModel) == std::string("precomputed"));
  CHECK(config.printJson);
  CHECK(config.printTree);
}

TEST_CASE("Config treats directed shorthand as directed flow model [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --directed", true);
  CHECK(infomap::flowModelToString(config.flowModel) == std::string("directed"));
}

TEST_CASE("Config rejects unknown flow models and output formats [fast][core][config][cli]")
{
  CHECK_THROWS_AS(Config("input.net --silent --no-file-output --flow-model unknown", true), std::runtime_error);
  CHECK_THROWS_AS(Config("input.net --silent --no-file-output --output tree,unknown", true), std::runtime_error);
}

TEST_CASE("Output format manifest describes CLI formats and compatible filenames [fast][core][config][cli]")
{
  const std::vector<std::string> expectedOptions = {
    "clu",
    "tree",
    "ftree",
    "newick",
    "json",
    "csv",
    "network",
    "states",
    "flow",
  };
  CHECK(infomap::outputFormatNames() == expectedOptions);

  std::map<std::string, std::vector<std::string>> expectedFiles = {
    { "clu", { "network.clu", "network_states.clu" } },
    { "tree", { "network.tree", "network_states.tree" } },
    { "ftree", { "network.ftree", "network_states.ftree" } },
    { "newick", { "network.nwk", "network_states.nwk" } },
    { "json", { "network.json", "network_states.json" } },
    { "csv", { "network.csv", "network_states.csv" } },
    { "network", { "network.net", "network_states_as_physical.net" } },
    { "states", { "network_states.net" } },
    { "flow", { "network_flow.net", "network_states_as_physical_flow.net" } },
  };

  for (const auto& optionName : expectedOptions) {
    const auto* format = infomap::findOutputFormat(optionName);
    REQUIRE(format != nullptr);
    std::vector<std::string> filenames;
    for (const auto& file : format->files) {
      filenames.push_back(infomap::outputFilename("network", file));
    }
    CHECK(filenames == expectedFiles[optionName]);
  }
}

TEST_CASE("Output format manifest result keys stay unique [fast][core][config][cli]")
{
  std::set<std::string> resultKeys;
  for (const auto& format : infomap::outputFormats()) {
    for (const auto& file : format.files) {
      CHECK(resultKeys.insert(file.resultKey).second);
    }
  }
  CHECK(resultKeys.count("json_states") == 1);
  CHECK(resultKeys.count("flow_as_physical") == 1);
}

} // namespace
