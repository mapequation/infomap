/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include <nlohmann/json.hpp>

#include "RunMetadata.h"
#include "Config.h"
#include "OutputPlan.h"
#include "../utils/format.h"
#include "../version.h"

#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <utility>
#include <vector>
#include <algorithm>

namespace infomap {

namespace {

  constexpr unsigned long long FNV_OFFSET = 14695981039346656037ull;
  constexpr unsigned long long FNV_PRIME = 1099511628211ull;
  using Json = nlohmann::ordered_json;

  template <typename T>
  void addCanonicalNumber(Json& json, const std::string& key, const T& value)
  {
    json[key] = value;
  }

  void hashBytes(unsigned long long& hash, const char* data, std::streamsize size)
  {
    for (std::streamsize i = 0; i < size; ++i) {
      hash ^= static_cast<unsigned char>(data[i]);
      hash *= FNV_PRIME;
    }
  }

  std::string fnvHex(const std::string& value)
  {
    auto hash = FNV_OFFSET;
    hashBytes(hash, value.data(), static_cast<std::streamsize>(value.size()));
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
  }

  std::string fileContentFingerprint(const std::string& path, unsigned long long size)
  {
    std::ifstream input(path.c_str(), std::ios_base::binary);
    if (!input) {
      throw std::runtime_error(fmt::format(FMT_STRING("Error opening input file '{}'. Check that the path points to a file and that you have read permissions."), path));
    }

    auto hash = FNV_OFFSET;
    const std::string sizeText = fmt::to_string(size);
    hashBytes(hash, sizeText.data(), static_cast<std::streamsize>(sizeText.size()));

    constexpr std::streamoff CHUNK_SIZE = 65536;
    std::vector<char> buffer(static_cast<std::size_t>(CHUNK_SIZE));

    const auto firstSize = static_cast<std::streamsize>(std::min<unsigned long long>(size, CHUNK_SIZE));
    input.read(buffer.data(), firstSize);
    hashBytes(hash, buffer.data(), input.gcount());

    if (size > static_cast<unsigned long long>(CHUNK_SIZE)) {
      input.clear();
      const auto tailOffset = static_cast<std::streamoff>(size - CHUNK_SIZE);
      input.seekg(tailOffset, std::ios_base::beg);
      input.read(buffer.data(), static_cast<std::streamsize>(CHUNK_SIZE));
      hashBytes(hash, buffer.data(), input.gcount());
    }

    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
  }

  std::string outputArtifactsJson(const Config& config)
  {
    Json json = Json::array();
    const auto appendArtifact = [&](const std::string& key, const std::string& path) {
      Json artifact;
      artifact["key"] = key;
      artifact["path"] = path;
      json.push_back(std::move(artifact));
    };
    for (auto phase : { OutputPhase::BeforeFlow, OutputPhase::AfterFlow, OutputPhase::AfterPartition }) {
      for (const auto& output : planOutputArtifacts(config, phase))
        appendArtifact(output.resultKey, output.filename);
    }
    for (const auto& report : planReportArtifacts(config))
      appendArtifact(report.first, report.second);
    return json.dump();
  }

} // namespace

std::string canonicalConfigJson(const Config& config)
{
  Json json;

  // Input identity (path, size, content) is captured separately by the input
  // fingerprint; the config fingerprint covers only algorithm-affecting
  // settings, so it stays stable when the same input is referenced via a
  // different path. The cluster/meta-data paths below have no separate content
  // fingerprint, so they remain the only available signal and are kept.
  addCanonicalNumber(json, "additional_input_count", config.additionalInput.size());
  json["flow_model"] = flowModelToString(config.flowModel);
  json["directed"] = config.directed;
  addCanonicalNumber(json, "seed", config.seedToRandomNumberGenerator);
  json["two_level"] = config.twoLevel;
  json["no_infomap"] = config.noInfomap;
  json["regularized"] = config.regularized;
  addCanonicalNumber(json, "regularization_strength", config.regularizationStrength);
  json["recorded_teleportation"] = config.recordedTeleportation;
  addCanonicalNumber(json, "teleportation_probability", config.teleportationProbability);
  addCanonicalNumber(json, "markov_time", config.markovTime);
  json["variable_markov_time"] = config.variableMarkovTime;
  addCanonicalNumber(json, "variable_markov_damping", config.variableMarkovTimeDamping);
  addCanonicalNumber(json, "variable_markov_min_scale", config.variableMarkovTimeMinLocalScale);
  json["entropy_corrected"] = config.entropyBiasCorrection;
  addCanonicalNumber(json, "entropy_correction_strength", config.entropyBiasCorrectionMultiplier);
  json["use_node_weights_as_flow"] = config.useNodeWeightsAsFlow;
  json["teleport_to_nodes"] = config.teleportToNodes;
  addCanonicalNumber(json, "weight_threshold", config.weightThreshold);
  json["no_self_links"] = config.noSelfLinks;
  addCanonicalNumber(json, "node_limit", config.nodeLimit);
  addCanonicalNumber(json, "matchable_multilayer_ids", config.matchableMultilayerIds);
  json["bipartite"] = config.bipartite;
  json["bipartite_teleportation"] = config.bipartiteTeleportation;
  json["cluster_data"] = config.clusterDataFile;
  json["meta_data"] = config.metaDataFile;
  addCanonicalNumber(json, "meta_data_rate", config.metaDataRate);
  json["meta_data_unweighted"] = config.unweightedMetaData;
  addCanonicalNumber(json, "preferred_number_of_modules", config.preferredNumberOfModules);
  json["parallel_trials"] = config.parallelTrials;
  json["inner_parallelization"] = config.innerParallelization;
  addCanonicalNumber(json, "core_loop_limit", config.coreLoopLimit);
  addCanonicalNumber(json, "core_level_limit", config.levelAggregationLimit);
  addCanonicalNumber(json, "tune_iteration_limit", config.tuneIterationLimit);
  addCanonicalNumber(json, "core_loop_codelength_threshold", config.minimumCodelengthImprovement);
  addCanonicalNumber(json, "tune_iteration_relative_threshold", config.minimumRelativeTuneIterationImprovement);
  addCanonicalNumber(json, "max_flow_iterations", config.maxFlowIterations);
  json["prefer_modular_solution"] = config.preferModularSolution;
  addCanonicalNumber(json, "num_random_moves", config.numRandomMoves);
  addCanonicalNumber(json, "max_degree_for_random_moves", config.maxDegreeForRandomMoves);
  json["skip_adjust_bipartite_flow"] = config.skipAdjustBipartiteFlow;
  json["markov_time_no_self_links"] = config.markovTimeNoSelfLinks;
  addCanonicalNumber(json, "multilayer_relax_rate", config.multilayerRelaxRate);
  addCanonicalNumber(json, "multilayer_relax_limit", config.multilayerRelaxLimit);
  addCanonicalNumber(json, "multilayer_relax_limit_up", config.multilayerRelaxLimitUp);
  addCanonicalNumber(json, "multilayer_relax_limit_down", config.multilayerRelaxLimitDown);
  addCanonicalNumber(json, "multilayer_js_relax_rate", config.multilayerJSRelaxRate);
  json["multilayer_relax_by_jsd"] = config.multilayerRelaxByJensenShannonDivergence;
  addCanonicalNumber(json, "multilayer_js_relax_limit", config.multilayerJSRelaxLimit);
  json["no_coarse_tune"] = config.noCoarseTune;
  json["only_super_modules"] = config.onlySuperModules;
  addCanonicalNumber(json, "fast_hierarchical_solution", config.fastHierarchicalSolution);
  json["randomize_core_loop_limit"] = config.randomizeCoreLoopLimit;
  addCanonicalNumber(json, "minimum_single_node_codelength_improvement", config.minimumSingleNodeCodelengthImprovement);

  return json.dump();
}

std::string configFingerprint(const Config& config)
{
  return fnvHex(canonicalConfigJson(config));
}

std::string inputFingerprintJson(const std::string& path)
{
  if (path.empty())
    return "null";

  struct stat info;
  if (stat(path.c_str(), &info) != 0) {
    throw std::runtime_error(fmt::format(FMT_STRING("Error reading input file metadata for '{}'."), path));
  }

  const auto size = static_cast<unsigned long long>(info.st_size);
  Json json;
  json["path"] = path;
  json["size"] = size;
  json["mtime"] = static_cast<long long>(info.st_mtime);
  json["hash"] = fileContentFingerprint(path, size);
  return json.dump();
}

std::string networkFingerprint(const std::string& path)
{
  if (path.empty())
    return "";

  struct stat info;
  if (stat(path.c_str(), &info) != 0) {
    throw std::runtime_error(fmt::format(FMT_STRING("Error reading input file metadata for '{}'."), path));
  }

  const auto size = static_cast<unsigned long long>(info.st_size);
  return fileContentFingerprint(path, size);
}

std::string runManifestJson(const Config& config)
{
  const auto canonicalConfig = canonicalConfigJson(config);
  Json json;
  json["version"] = INFOMAP_VERSION;
  json["command"] = config.parsedString;
  json["num_trials"] = config.numTrials;
  json["config"] = Json::parse(canonicalConfig);
  json["config_fingerprint"] = fnvHex(canonicalConfig);
  json["input"] = Json::parse(inputFingerprintJson(config.networkFile));
  json["outputs"] = Json::parse(outputArtifactsJson(config));
  return json.dump() + '\n';
}

} // namespace infomap
