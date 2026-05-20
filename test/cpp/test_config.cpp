#include "vendor/doctest.h"

#include "io/ConfigBuilder.h"
#include "io/Config.h"
#include "io/OutputFormats.h"
#include "io/ParameterCatalog.h"

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

  const Config aliasPrecedence("input.net --silent --no-file-output --flow-model undirected --directed", true);
  CHECK(infomap::flowModelToString(aliasPrecedence.flowModel) == std::string("directed"));
}

TEST_CASE("Config adapts parsed runtime defaults after registration [fast][core][config][cli]")
{
  const Config regularized("input.net --silent --no-file-output --regularized", true);
  CHECK(regularized.regularized);
  CHECK(regularized.recordedTeleportation);

  const Config noInfomap("input.net --silent --no-file-output --no-infomap --num-trials 4", true);
  CHECK(noInfomap.noInfomap);
  CHECK(noInfomap.numTrials == 1);

  const Config libraryConfig("--silent", false);
  CHECK(libraryConfig.noFileOutput);
  CHECK(libraryConfig.outDirectory.empty());
}

TEST_CASE("ConfigBuilder exposes raw parse state before runtime adaptation [fast][core][config][cli]")
{
  Config raw;
  raw.isCLI = true;

  const auto parsed = infomap::ConfigBuilder::parseRaw(raw, "input.net --silent --no-file-output --flow-model directed --regularized --num-trials 4", true);
  CHECK(raw.networkFile == "input.net");
  CHECK(raw.noFileOutput);
  CHECK(raw.regularized);
  CHECK_FALSE(raw.recordedTeleportation);
  CHECK(raw.numTrials == 4);
  CHECK(parsed.flowModelArg == "directed");
  CHECK_FALSE(parsed.usedOptions.empty());

  infomap::ConfigBuilder::applyParsed(raw, parsed, true);
  CHECK(infomap::flowModelToString(raw.flowModel) == std::string("directed"));
  CHECK(raw.recordedTeleportation);
}

TEST_CASE("Config parses pretty output flag [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --pretty", true);
  CHECK(config.prettyOutput);

  const Config negatedConfig("input.net --silent --no-file-output --pretty --no-pretty", true);
  CHECK_FALSE(negatedConfig.prettyOutput);
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

TEST_CASE("Parameter catalog owns option choices and binding names [fast][core][config][cli]")
{
  const infomap::ParameterSpec* output = nullptr;
  const infomap::ParameterSpec* verbose = nullptr;
  const infomap::ParameterSpec* teleportation = nullptr;
  const infomap::ParameterSpec* numTrials = nullptr;
  for (const auto& parameter : infomap::parameterCatalog()) {
    if (parameter.longName == "output") {
      output = &parameter;
    }
    if (parameter.longName == "verbose") {
      verbose = &parameter;
    }
    if (parameter.longName == "teleportation-probability") {
      teleportation = &parameter;
    }
    if (parameter.longName == "num-trials") {
      numTrials = &parameter;
    }
  }

  REQUIRE(output != nullptr);
  CHECK(output->choices == infomap::outputFormatNames());
  CHECK(output->renderPolicy == "comma_list");

  REQUIRE(verbose != nullptr);
  CHECK(verbose->pythonName == "verbosity_level");
  CHECK(verbose->rName == "verbosity_level");
  CHECK(verbose->tsName == "verbose");
  CHECK(verbose->renderPolicy == "repeated_short");

  REQUIRE(teleportation != nullptr);
  CHECK(teleportation->minValue == "0");
  CHECK(teleportation->maxValue == "1");

  REQUIRE(numTrials != nullptr);
  CHECK(numTrials->minValue == "1");
}

} // namespace
