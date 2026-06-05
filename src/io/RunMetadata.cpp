/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "RunMetadata.h"
#include "Config.h"
#include "OutputPlan.h"
#include "../utils/convert.h"
#include "../version.h"

#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <vector>
#include <algorithm>

namespace infomap {

namespace {

  constexpr unsigned long long FNV_OFFSET = 14695981039346656037ull;
  constexpr unsigned long long FNV_PRIME = 1099511628211ull;

  std::string jsonString(const std::string& value)
  {
    std::ostringstream escaped;
    escaped << '"';
    for (char c : value) {
      switch (c) {
      case '"':
      case '\\':
        escaped << '\\' << c;
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          // Remaining control characters must be \u-escaped to stay valid JSON.
          escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<int>(static_cast<unsigned char>(c))
                  << std::dec << std::setfill(' ');
        } else {
          escaped << c;
        }
        break;
      }
    }
    escaped << '"';
    return escaped.str();
  }

  std::string boolString(bool value)
  {
    return value ? "true" : "false";
  }

  void appendField(std::ostringstream& json, bool& first, const std::string& key, const std::string& value)
  {
    if (!first)
      json << ',';
    first = false;
    json << jsonString(key) << ':' << value;
  }

  void appendStringField(std::ostringstream& json, bool& first, const std::string& key, const std::string& value)
  {
    appendField(json, first, key, jsonString(value));
  }

  // Render numbers with enough precision to round-trip an IEEE double, on a
  // dedicated stream. io::Str() / the JSON stream use default precision (6),
  // which would collapse distinct doubles to the same string and collide in
  // the fingerprint. setprecision is ignored for integral types.
  template <typename T>
  std::string numberToString(const T& value)
  {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return out.str();
  }

  template <typename T>
  void appendNumericField(std::ostringstream& json, bool& first, const std::string& key, const T& value)
  {
    appendField(json, first, key, numberToString(value));
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
      throw std::runtime_error(io::Str() << "Error opening input file '" << path << "'. Check that the path points to a file and that you have read permissions.");
    }

    auto hash = FNV_OFFSET;
    const std::string sizeText = io::Str() << size;
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
    std::ostringstream json;
    json << '[';
    bool first = true;
    const auto appendArtifact = [&](const std::string& key, const std::string& path) {
      if (!first)
        json << ',';
      first = false;
      json << "{\"key\":" << jsonString(key) << ",\"path\":" << jsonString(path) << '}';
    };
    for (auto phase : { OutputPhase::BeforeFlow, OutputPhase::AfterFlow, OutputPhase::AfterPartition }) {
      for (const auto& output : planOutputArtifacts(config, phase))
        appendArtifact(output.resultKey, output.filename);
    }
    for (const auto& report : planReportArtifacts(config))
      appendArtifact(report.first, report.second);
    json << ']';
    return json.str();
  }

} // namespace

std::string canonicalConfigJson(const Config& config)
{
  std::ostringstream json;
  json << '{';
  bool first = true;

  // Input identity (path, size, content) is captured separately by the input
  // fingerprint; the config fingerprint covers only algorithm-affecting
  // settings, so it stays stable when the same input is referenced via a
  // different path. The cluster/meta-data paths below have no separate content
  // fingerprint, so they remain the only available signal and are kept.
  appendField(json, first, "additional_input_count", numberToString(config.additionalInput.size()));
  appendField(json, first, "flow_model", jsonString(flowModelToString(config.flowModel)));
  appendField(json, first, "directed", boolString(config.directed));
  appendNumericField(json, first, "seed", config.seedToRandomNumberGenerator);
  appendField(json, first, "two_level", boolString(config.twoLevel));
  appendField(json, first, "no_infomap", boolString(config.noInfomap));
  appendField(json, first, "regularized", boolString(config.regularized));
  appendNumericField(json, first, "regularization_strength", config.regularizationStrength);
  appendField(json, first, "recorded_teleportation", boolString(config.recordedTeleportation));
  appendNumericField(json, first, "teleportation_probability", config.teleportationProbability);
  appendNumericField(json, first, "markov_time", config.markovTime);
  appendField(json, first, "variable_markov_time", boolString(config.variableMarkovTime));
  appendNumericField(json, first, "variable_markov_damping", config.variableMarkovTimeDamping);
  appendNumericField(json, first, "variable_markov_min_scale", config.variableMarkovTimeMinLocalScale);
  appendField(json, first, "entropy_corrected", boolString(config.entropyBiasCorrection));
  appendNumericField(json, first, "entropy_correction_strength", config.entropyBiasCorrectionMultiplier);
  appendField(json, first, "use_node_weights_as_flow", boolString(config.useNodeWeightsAsFlow));
  appendField(json, first, "teleport_to_nodes", boolString(config.teleportToNodes));
  appendNumericField(json, first, "weight_threshold", config.weightThreshold);
  appendField(json, first, "no_self_links", boolString(config.noSelfLinks));
  appendNumericField(json, first, "node_limit", config.nodeLimit);
  appendNumericField(json, first, "matchable_multilayer_ids", config.matchableMultilayerIds);
  appendField(json, first, "bipartite", boolString(config.bipartite));
  appendField(json, first, "bipartite_teleportation", boolString(config.bipartiteTeleportation));
  appendStringField(json, first, "cluster_data", config.clusterDataFile);
  appendStringField(json, first, "meta_data", config.metaDataFile);
  appendNumericField(json, first, "meta_data_rate", config.metaDataRate);
  appendField(json, first, "meta_data_unweighted", boolString(config.unweightedMetaData));
  appendNumericField(json, first, "preferred_number_of_modules", config.preferredNumberOfModules);
  appendField(json, first, "parallel_trials", boolString(config.parallelTrials));
  appendField(json, first, "inner_parallelization", boolString(config.innerParallelization));
  appendNumericField(json, first, "core_loop_limit", config.coreLoopLimit);
  appendNumericField(json, first, "core_level_limit", config.levelAggregationLimit);
  appendNumericField(json, first, "tune_iteration_limit", config.tuneIterationLimit);
  appendNumericField(json, first, "core_loop_codelength_threshold", config.minimumCodelengthImprovement);
  appendNumericField(json, first, "tune_iteration_relative_threshold", config.minimumRelativeTuneIterationImprovement);
  appendNumericField(json, first, "max_flow_iterations", config.maxFlowIterations);
  appendField(json, first, "prefer_modular_solution", boolString(config.preferModularSolution));
  appendNumericField(json, first, "num_random_moves", config.numRandomMoves);
  appendNumericField(json, first, "max_degree_for_random_moves", config.maxDegreeForRandomMoves);
  appendField(json, first, "skip_adjust_bipartite_flow", boolString(config.skipAdjustBipartiteFlow));
  appendField(json, first, "markov_time_no_self_links", boolString(config.markovTimeNoSelfLinks));
  appendNumericField(json, first, "multilayer_relax_rate", config.multilayerRelaxRate);
  appendNumericField(json, first, "multilayer_relax_limit", config.multilayerRelaxLimit);
  appendNumericField(json, first, "multilayer_relax_limit_up", config.multilayerRelaxLimitUp);
  appendNumericField(json, first, "multilayer_relax_limit_down", config.multilayerRelaxLimitDown);
  appendNumericField(json, first, "multilayer_js_relax_rate", config.multilayerJSRelaxRate);
  appendField(json, first, "multilayer_relax_by_jsd", boolString(config.multilayerRelaxByJensenShannonDivergence));
  appendNumericField(json, first, "multilayer_js_relax_limit", config.multilayerJSRelaxLimit);
  appendField(json, first, "no_coarse_tune", boolString(config.noCoarseTune));
  appendField(json, first, "only_super_modules", boolString(config.onlySuperModules));
  appendNumericField(json, first, "fast_hierarchical_solution", config.fastHierarchicalSolution);
  appendField(json, first, "randomize_core_loop_limit", boolString(config.randomizeCoreLoopLimit));
  appendNumericField(json, first, "minimum_single_node_codelength_improvement", config.minimumSingleNodeCodelengthImprovement);

  json << '}';
  return json.str();
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
    throw std::runtime_error(io::Str() << "Error reading input file metadata for '" << path << "'.");
  }

  const auto size = static_cast<unsigned long long>(info.st_size);
  std::ostringstream json;
  json << "{\"path\":" << jsonString(path)
       << ",\"size\":" << size
       << ",\"mtime\":" << static_cast<long long>(info.st_mtime)
       << ",\"hash\":" << jsonString(fileContentFingerprint(path, size))
       << '}';
  return json.str();
}

std::string runManifestJson(const Config& config)
{
  const auto canonicalConfig = canonicalConfigJson(config);
  std::ostringstream json;
  json << '{'
       << "\"version\":" << jsonString(INFOMAP_VERSION) << ','
       << "\"command\":" << jsonString(config.parsedString) << ','
       << "\"config\":" << canonicalConfig << ','
       << "\"config_fingerprint\":" << jsonString(fnvHex(canonicalConfig)) << ','
       << "\"input\":" << inputFingerprintJson(config.networkFile) << ','
       << "\"outputs\":" << outputArtifactsJson(config)
       << "}\n";
  return json.str();
}

} // namespace infomap
