#include "vendor/doctest.h"

#include "io/ConfigBuilder.h"
#include "io/Config.h"
#include "io/OutputFormats.h"
#include "io/OutputPlan.h"
#include "io/ParameterCatalog.h"
#include "io/ProgramInterface.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

namespace {

using infomap::Config;

std::vector<std::string> resultKeysFor(const Config& config, infomap::OutputPhase phase)
{
  std::vector<std::string> keys;
  for (const auto& artifact : infomap::planOutputArtifacts(config, phase)) {
    keys.push_back(artifact.resultKey);
  }
  return keys;
}

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

TEST_CASE("Flow model names map to runtime values [fast][core][config][cli]")
{
  const std::vector<std::pair<std::string, int>> expected = {
    { "undirected", static_cast<int>(infomap::FlowModel::undirected) },
    { "directed", static_cast<int>(infomap::FlowModel::directed) },
    { "undirdir", static_cast<int>(infomap::FlowModel::undirdir) },
    { "outdirdir", static_cast<int>(infomap::FlowModel::outdirdir) },
    { "rawdir", static_cast<int>(infomap::FlowModel::rawdir) },
    { "precomputed", static_cast<int>(infomap::FlowModel::precomputed) },
  };

  CHECK(infomap::flowModelNames().size() == expected.size());
  for (const auto& entry : expected) {
    infomap::FlowModel parsed = infomap::FlowModel::undirected;
    CHECK(infomap::parseFlowModel(entry.first, parsed));
    CHECK(parsed == entry.second);
    CHECK(infomap::flowModelToString(parsed) == entry.first);
  }
}

TEST_CASE("Parameter catalog flow model choices match runtime flow model names [fast][core][config][cli]")
{
  const auto& expected = infomap::flowModelNames();
  bool foundFlowModel = false;

  for (const auto& parameter : infomap::parameterCatalog()) {
    if (parameter.longName == "flow-model") {
      foundFlowModel = true;
      CHECK(parameter.choices == expected);
    }
  }

  CHECK(foundFlowModel);
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

TEST_CASE("Config accepts zero sentinel limits [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --core-level-limit 0 --tune-iteration-limit 0", true);
  CHECK(config.levelAggregationLimit == 0);
  CHECK(config.tuneIterationLimit == 0);
}

TEST_CASE("Config rejects unknown flow models and output formats [fast][core][config][cli]")
{
  CHECK_THROWS_WITH_AS(Config("input.net --silent --no-file-output --flow-model unknown", true), "Unrecognized flow model: 'unknown'", std::runtime_error);
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

TEST_CASE("Output plan expands first-order modular artifacts [fast][core][config][output]")
{
  Config config;
  config.outName = "network";
  config.printTree = true;

  const auto artifacts = infomap::planOutputArtifacts(config, infomap::OutputPhase::AfterPartition);

  REQUIRE(artifacts.size() == 1);
  CHECK(artifacts.front().resultKey == "tree");
  CHECK(artifacts.front().label == "tree");
  CHECK(artifacts.front().filename == "network.tree");
  CHECK(artifacts.front().phase == infomap::OutputPhase::AfterPartition);
  CHECK(artifacts.front().kind == infomap::OutputKind::Tree);
  CHECK_FALSE(artifacts.front().states);
}

TEST_CASE("Output plan expands state and physical modular artifacts [fast][core][config][output]")
{
  Config config;
  config.outName = "network";
  config.printTree = true;
  config.setStateOutput();

  const auto artifacts = infomap::planOutputArtifacts(config, infomap::OutputPhase::AfterPartition);

  REQUIRE(artifacts.size() == 2);
  CHECK(resultKeysFor(config, infomap::OutputPhase::AfterPartition) == std::vector<std::string> { "tree", "tree_states" });
  CHECK(artifacts[0].label == "physical tree");
  CHECK(artifacts[0].filename == "network.tree");
  CHECK_FALSE(artifacts[0].states);
  CHECK(artifacts[1].label == "state tree");
  CHECK(artifacts[1].filename == "network_states.tree");
  CHECK(artifacts[1].states);
}

TEST_CASE("Output plan assigns network artifacts to write phases [fast][core][config][output]")
{
  Config config;
  config.outName = "network";
  config.printClu = true;
  config.printTree = true;
  config.printFlowNetwork = true;
  config.printPajekNetwork = true;
  config.printStateNetwork = true;
  config.setStateOutput();

  CHECK(resultKeysFor(config, infomap::OutputPhase::BeforeFlow) == std::vector<std::string> { "states", "states_as_physical" });
  CHECK(resultKeysFor(config, infomap::OutputPhase::AfterFlow) == std::vector<std::string> { "flow_as_physical" });
  CHECK(resultKeysFor(config, infomap::OutputPhase::AfterPartition) == std::vector<std::string> { "tree", "tree_states", "clu", "clu_states" });

  for (const auto phase : { infomap::OutputPhase::BeforeFlow, infomap::OutputPhase::AfterFlow, infomap::OutputPhase::AfterPartition }) {
    for (const auto& artifact : infomap::planOutputArtifacts(config, phase)) {
      CHECK(artifact.phase == phase);
      CHECK(infomap::findOutputFileFormat(artifact.resultKey) != nullptr);
    }
  }
}

TEST_CASE("Output plan applies trial suffix before format suffix [fast][core][config][output]")
{
  Config config;
  config.outDirectory = "out/";
  config.outName = "network";
  config.printTree = true;
  config.printAllTrials = true;
  config.numTrials = 3;

  const auto artifacts = infomap::planOutputArtifacts(config, infomap::OutputPhase::AfterPartition, 2);

  REQUIRE(artifacts.size() == 1);
  CHECK(infomap::outputPlanBasename(config, 2) == "out/network_trial_2");
  CHECK(artifacts.front().filename == "out/network_trial_2.tree");
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

  const auto findParameter = [](const std::string& longName) {
    for (const auto& parameter : infomap::parameterCatalog()) {
      if (parameter.longName == longName) {
        return &parameter;
      }
    }
    return static_cast<const infomap::ParameterSpec*>(nullptr);
  };

  const auto* coreLevelLimit = findParameter("core-level-limit");
  REQUIRE(coreLevelLimit != nullptr);
  CHECK(coreLevelLimit->defaultValue == "0");
  CHECK(coreLevelLimit->minValue == "0");

  const auto* tuneIterationLimit = findParameter("tune-iteration-limit");
  REQUIRE(tuneIterationLimit != nullptr);
  CHECK(tuneIterationLimit->defaultValue == "0");
  CHECK(tuneIterationLimit->minValue == "0");
}

TEST_CASE("Parameter catalog registers with ProgramInterface through a named adapter [fast][core][config][cli]")
{
  Config config;
  infomap::ParsedParameterSet parsed;
  infomap::ProgramInterface api("Infomap", "Test parser", "1.0");

  infomap::registerCatalogWithProgramInterface(api, { config, parsed }, true);
  api.parseArgs("input.net --silent --no-file-output --flow-model directed");

  CHECK(config.networkFile == "input.net");
  CHECK(config.silent);
  CHECK(config.noFileOutput);
  CHECK(parsed.flowModelArg == "directed");
  CHECK_FALSE(api.getUsedOptionArguments().empty());
}

} // namespace
