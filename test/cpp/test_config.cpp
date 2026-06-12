#include "vendor/doctest.h"

#include "TestUtils.h"
#include "io/Config.h"
#include "io/OutputFormats.h"
#include "io/OutputPlan.h"
#include "io/ParameterCatalog.h"
#include "io/ProgramInterface.h"
#include "io/RunMetadata.h"

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

const infomap::ParameterSpec* findParameter(const std::string& longName)
{
  for (const auto& parameter : infomap::parameterCatalog()) {
    if (parameter.longName == longName) {
      return &parameter;
    }
  }
  return nullptr;
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

  const auto* flowModel = findParameter("flow-model");
  REQUIRE(flowModel != nullptr);
  CHECK(flowModel->choices == expected);
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

TEST_CASE("Config construction applies cross-field invariants from flags [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --flow-model directed --regularized --num-trials 4", true);

  CHECK(config.isCLI);
  CHECK(config.networkFile == "input.net");
  CHECK(config.noFileOutput);
  CHECK(config.regularized);
  CHECK(config.numTrials == 4);
  CHECK(infomap::flowModelToString(config.flowModel) == std::string("directed"));
  // applyOptionInteractions: regularized implies recordedTeleportation
  CHECK(config.recordedTeleportation);
  CHECK_FALSE(config.parsedOptions.empty());
}

TEST_CASE("Config construction applies runtime output interactions [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --verbose --pretty --print-all-trials --num-trials 1 --output json", true);

  CHECK(config.isCLI);
  CHECK(config.parsedString == "input.net --silent --no-file-output --verbose --pretty --print-all-trials --num-trials 1 --output json");
  CHECK_FALSE(config.parsedOptions.empty());
  CHECK(config.verbosity == 1);
  // Pretty is the base layer at every verbosity; -v adds diagnostics on top
  // rather than disabling pretty.
  CHECK(config.prettyOutput);
  // applyRuntimeOutputInteractions: printAllTrials requires numTrials >= 2
  CHECK_FALSE(config.printAllTrials);
  CHECK(config.printJson);
  CHECK_FALSE(config.printTree);
  // applyOutputNameDefault: outName derives from networkFile when not set
  CHECK(config.outName == "input");
}

TEST_CASE("adaptDefaults applies invariants when called on a library-mutated config [fast][core][config][library]")
{
  Config config;
  config.outputFormats = "tree";
  config.regularized = true;
  config.noInfomap = true;
  config.numTrials = 7;
  config.networkFile = "graph.net";

  config.adaptDefaults();

  // applyLibraryOutputDefaults: non-CLI with empty outDirectory implies noFileOutput
  CHECK(config.noFileOutput);
  // applyOptionInteractions: regularized -> recordedTeleportation, noInfomap -> numTrials = 1
  CHECK(config.recordedTeleportation);
  CHECK(config.numTrials == 1);
  // applyOutputNameDefault: outName derives from networkFile
  CHECK(config.outName == "graph");
}

TEST_CASE("Pretty output is always on; --pretty/--no-pretty are deprecated no-ops [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --pretty", true);
  CHECK(config.prettyOutput);

  // --no-pretty is a deprecated no-op: pretty output stays enabled.
  const Config negatedConfig("input.net --silent --no-file-output --no-pretty", true);
  CHECK(negatedConfig.prettyOutput);

  // The default (no flag at all) is pretty as well.
  const Config defaultConfig("input.net --silent --no-file-output", true);
  CHECK(defaultConfig.prettyOutput);
}

TEST_CASE("Config parses parallel trials flag [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --num-trials 4 --parallel-trials", true);
  CHECK(config.numTrials == 4);
  CHECK(config.parallelTrials);

  const auto* parameter = findParameter("parallel-trials");
  REQUIRE(parameter != nullptr);
  CHECK(parameter->group == "Accuracy");
  CHECK(parameter->isAdvanced);
  CHECK_FALSE(parameter->requireArgument);
}

TEST_CASE("Config parses run report flags [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --timing-json timing.json --summary-json summary.json --memory-report --manifest-json manifest.json", true);

  CHECK(config.timingJsonPath == "timing.json");
  CHECK(config.summaryJsonPath == "summary.json");
  CHECK(config.memoryReport);
  CHECK(config.runManifestPath == "manifest.json");

  const auto* timingJson = findParameter("timing-json");
  REQUIRE(timingJson != nullptr);
  CHECK(timingJson->group == "Output");
  CHECK(timingJson->isAdvanced);
  CHECK(timingJson->requireArgument);

  const auto* summaryJson = findParameter("summary-json");
  REQUIRE(summaryJson != nullptr);
  CHECK(summaryJson->group == "Output");
  CHECK(summaryJson->isAdvanced);
  CHECK(summaryJson->requireArgument);

  const auto* memoryReport = findParameter("memory-report");
  REQUIRE(memoryReport != nullptr);
  CHECK(memoryReport->group == "Output");
  CHECK(memoryReport->isAdvanced);
  CHECK_FALSE(memoryReport->requireArgument);

  const auto* runManifest = findParameter("manifest-json");
  REQUIRE(runManifest != nullptr);
  CHECK(runManifest->group == "Output");
  CHECK(runManifest->isAdvanced);
  CHECK(runManifest->requireArgument);
}

TEST_CASE("Config parses batch output safety flags [fast][core][config][cli]")
{
  const Config defaultConfig("input.net --silent --no-file-output", true);
  CHECK(defaultConfig.overwriteOutput());

  const Config strictConfig("input.net --silent --no-file-output --no-overwrite", true);
  CHECK_FALSE(strictConfig.overwriteOutput());

  // --overwrite was removed; only --no-overwrite opts into strict mode.
  CHECK_THROWS_AS(Config("input.net --silent --no-file-output --overwrite", true), std::runtime_error);

  const auto* noOverwrite = findParameter("no-overwrite");
  REQUIRE(noOverwrite != nullptr);
  CHECK(noOverwrite->group == "Output");
  CHECK(noOverwrite->isAdvanced);
  CHECK_FALSE(noOverwrite->requireArgument);

  CHECK(findParameter("overwrite") == nullptr);
}

TEST_CASE("Config fingerprint ignores presentation output options [fast][core][config][cli]")
{
  const Config base("input.net --silent --no-file-output --seed 7 --num-trials 2 --flow-model directed", true);
  const Config cosmetic("input.net --pretty --verbose --no-file-output --seed 7 --num-trials 2 --flow-model directed --timing-json timing.json --summary-json summary.json", true);
  const Config changed("input.net --silent --no-file-output --seed 8 --num-trials 2 --flow-model directed", true);

  CHECK(infomap::configFingerprint(base) == infomap::configFingerprint(cosmetic));
  CHECK(infomap::configFingerprint(base) != infomap::configFingerprint(changed));
  CHECK(infomap::canonicalConfigJson(base).find("\"seed\":7") != std::string::npos);
}

TEST_CASE("Config fingerprint ignores the input path [fast][core][config][cli]")
{
  // Input identity is captured by the input fingerprint, not the config
  // fingerprint, so the same algorithm config over a different path matches.
  const Config a("a.net --silent --no-file-output --seed 7 --num-trials 2", true);
  const Config b("sub/dir/b.net --silent --no-file-output --seed 7 --num-trials 2", true);

  CHECK(infomap::configFingerprint(a) == infomap::configFingerprint(b));
  CHECK(infomap::canonicalConfigJson(a).find(".net") == std::string::npos);
}

TEST_CASE("Config fingerprint round-trips double precision [fast][core][config][cli]")
{
  // Default stream precision (6) would collapse these to the same string.
  const Config base("input.net --silent --no-file-output --markov-time 1.0", true);
  const Config nudged("input.net --silent --no-file-output --markov-time 1.0000000001", true);

  CHECK(infomap::configFingerprint(base) != infomap::configFingerprint(nudged));
}

TEST_CASE("Config fingerprint tracks multilayer relax settings [fast][core][config][cli]")
{
  const Config base("input.net --silent --no-file-output", true);
  const Config relaxed("input.net --silent --no-file-output --multilayer-relax-rate 0.3", true);

  CHECK(infomap::configFingerprint(base) != infomap::configFingerprint(relaxed));
}

TEST_CASE("Config parses fingerprint-only CLI flag [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --print-config-fingerprint", true);

  CHECK(config.printConfigFingerprint);

  const auto* parameter = findParameter("print-config-fingerprint");
  REQUIRE(parameter != nullptr);
  CHECK(parameter->group == "Output");
  CHECK(parameter->isAdvanced);
  CHECK_FALSE(parameter->requireArgument);
}

TEST_CASE("Config rejects two stdout run report streams [fast][core][config][cli]")
{
  CHECK_THROWS_WITH_AS(
      Config("input.net --silent --no-file-output --timing-json - --summary-json -", true),
      "--timing-json - and --summary-json - cannot both write to stdout",
      std::runtime_error);
  CHECK_THROWS_WITH_AS(
      Config("input.net --silent --no-file-output --summary-json - --manifest-json -", true),
      "--summary-json - and --manifest-json - cannot both write to stdout",
      std::runtime_error);
}

TEST_CASE("Config rejects memory report without timing JSON [fast][core][config][cli]")
{
  CHECK_THROWS_WITH_AS(
      Config("input.net --silent --no-file-output --memory-report", true),
      "--memory-report requires --timing-json",
      std::runtime_error);

  CHECK_THROWS_WITH_AS(
      Config("input.net --silent --no-file-output --summary-json summary.json --memory-report", true),
      "--memory-report requires --timing-json",
      std::runtime_error);
}

TEST_CASE("Config requires silent mode for stdout run reports [fast][core][config][cli]")
{
  CHECK_THROWS_WITH_AS(
      Config("input.net --no-file-output --timing-json -", true),
      "--timing-json - requires --silent",
      std::runtime_error);
  CHECK_THROWS_WITH_AS(
      Config("input.net --no-file-output --summary-json -", true),
      "--summary-json - requires --silent",
      std::runtime_error);
  CHECK_THROWS_WITH_AS(
      Config("input.net --no-file-output --manifest-json -", true),
      "--manifest-json - requires --silent",
      std::runtime_error);

  CHECK_NOTHROW(Config("input.net --silent --no-file-output --timing-json -", true));
  CHECK_NOTHROW(Config("input.net --silent --no-file-output --summary-json -", true));
  CHECK_NOTHROW(Config("input.net --silent --no-file-output --manifest-json -", true));
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

TEST_CASE("Feature-gated option follows compile-time flag [fast][core][config][cli]")
{
#if INFOMAP_FEATURE_TEST_FEATURE
  CHECK(findParameter("test-feature") != nullptr);

  const Config config("input.net --silent --no-file-output --test-feature", true);
  CHECK(config.testFeature);
#else
  CHECK(findParameter("test-feature") == nullptr);
  CHECK_THROWS_AS(Config("input.net --silent --no-file-output --test-feature", true), std::runtime_error);
#endif
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

TEST_CASE("Config parses --num-threads as an explicit budget [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --num-threads 4", true);
  CHECK(config.numThreads == 4);
}

TEST_CASE("Config accepts --threads as an alias for --num-threads [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --threads 6", true);
  CHECK(config.numThreads == 6);
}

TEST_CASE("Config treats --num-threads auto as the zero sentinel [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --num-threads auto", true);
  CHECK(config.numThreads == 0);
}

TEST_CASE("Config defaults numThreads to auto when flag absent [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output", true);
  CHECK(config.numThreads == 0);
}

TEST_CASE("Config rejects non-numeric, non-auto --num-threads [fast][core][config][cli]")
{
  CHECK_THROWS(Config("input.net --silent --no-file-output --num-threads banana", true));
}

TEST_CASE("Config rejects --num-threads 0 [fast][core][config][cli]")
{
  CHECK_THROWS(Config("input.net --silent --no-file-output --num-threads 0", true));
}

TEST_CASE("Config rejects an out-of-range --num-threads without crashing [fast][core][config][cli]")
{
  CHECK_THROWS(Config("input.net --silent --no-file-output --num-threads 999999999999999999999999", true));
}

TEST_CASE("Config rejects a negative --num-threads [fast][core][config][cli]")
{
  CHECK_THROWS(Config("input.net --silent --no-file-output --num-threads -1", true));
}

TEST_CASE("Config rejects --num-threads with trailing garbage [fast][core][config][cli]")
{
  CHECK_THROWS(Config("input.net --silent --no-file-output --num-threads 4x", true));
}

TEST_CASE("Config fingerprint is invariant to num_trials and trial offset [fast][core][config][cli]")
{
  const Config a("input.net --silent --no-file-output --num-trials 1", true);
  const Config b("input.net --silent --no-file-output --num-trials 100", true);
  CHECK(infomap::configFingerprint(a) == infomap::configFingerprint(b));
  CHECK(infomap::canonicalConfigJson(a).find("num_trials") == std::string::npos);
}

TEST_CASE("networkFingerprint depends only on content [fast][core][config]")
{
  // Two files with different content must hash differently.
  const auto pathA = infomap::test::networkFixturePath("accumulate_a.net");
  const auto pathB = infomap::test::networkFixturePath("accumulate_b.net");
  CHECK_FALSE(infomap::networkFingerprint(pathA).empty());
  CHECK(infomap::networkFingerprint(pathA) != infomap::networkFingerprint(pathB));

  // The same file read twice must give the same hash (content-stable).
  CHECK(infomap::networkFingerprint(pathA) == infomap::networkFingerprint(pathA));
}

TEST_CASE("Config parses --trial-offset [fast][core][config][cli]")
{
  const Config c("input.net --silent --no-file-output --trial-offset 25", true);
  CHECK(c.trialOffset == 25);
}

TEST_CASE("Config parses --trial-results and --no-final-output [fast][core][config][cli]")
{
  const Config c("input.net --silent --no-file-output --trial-results r.json --no-final-output", true);
  CHECK(c.trialResultsPath == "r.json");
  CHECK(c.noFinalOutput);
}


} // namespace
