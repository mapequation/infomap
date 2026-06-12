/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "OutputPlan.h"
#include "InfomapError.h"
#include "Network.h"
#include "OutputFormats.h"
#include "SafeFile.h"
#include "../core/InfomapBase.h"
#include "../utils/Console.h"
#include "../utils/convert.h"
#include "../utils/format.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace infomap {

namespace {

  OutputArtifact artifact(const Config& config,
                          const std::string& basename,
                          OutputPhase phase,
                          OutputKind kind,
                          const std::string& resultKey,
                          const std::string& label,
                          bool states = false,
                          bool printFlow = false)
  {
    OutputArtifact output;
    output.resultKey = resultKey;
    output.label = label;
    output.filename = outputFilenameForResultKey(basename, resultKey);
    output.phase = phase;
    output.kind = kind;
    output.states = states;
    output.cluLevel = config.cluLevel;
    output.printFlow = printFlow;
    return output;
  }

  void addPhysicalAndStateArtifacts(std::vector<OutputArtifact>& artifacts,
                                    const Config& config,
                                    const std::string& basename,
                                    OutputKind kind,
                                    const std::string& physicalResultKey,
                                    const std::string& stateResultKey,
                                    const std::string& label,
                                    const std::string& physicalLabel,
                                    const std::string& stateLabel)
  {
    if (!config.printStates()) {
      artifacts.push_back(artifact(config, basename, OutputPhase::AfterPartition, kind, physicalResultKey, label));
      return;
    }

    artifacts.push_back(artifact(config, basename, OutputPhase::AfterPartition, kind, physicalResultKey, physicalLabel));
    artifacts.push_back(artifact(config, basename, OutputPhase::AfterPartition, kind, stateResultKey, stateLabel, true));
  }

  std::pair<std::string, std::string> splitExtension(const std::string& filename)
  {
    const auto slashPos = filename.find_last_of("/\\");
    const auto dotPos = filename.find_last_of('.');
    if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos)) {
      return { filename, "" };
    }
    return { filename.substr(0, dotPos), filename.substr(dotPos + 1) };
  }

  std::vector<std::string> summarizeOutputFiles(const std::vector<std::pair<std::string, std::string>>& outputFiles)
  {
    std::vector<std::pair<std::string, std::vector<std::string>>> groups;
    for (const auto& outputFile : outputFiles) {
      const auto parts = splitExtension(outputFile.second);
      auto it = std::find_if(groups.begin(), groups.end(), [&parts](const std::pair<std::string, std::vector<std::string>>& group) {
        return group.first == parts.first;
      });
      if (it == groups.end()) {
        groups.push_back({ parts.first, { parts.second } });
      } else {
        it->second.push_back(parts.second);
      }
    }

    std::vector<std::string> summaries;
    summaries.reserve(groups.size());
    for (const auto& group : groups) {
      if (group.second.size() == 1) {
        summaries.push_back(group.second.front().empty() ? group.first : fmt::format(FMT_STRING("{}.{}"), group.first, group.second.front()));
        continue;
      }
      summaries.push_back(fmt::format(FMT_STRING("{}.{{{}}}"), group.first, io::stringify(group.second, ",")));
    }
    return summaries;
  }

  void writeModularOutput(InfomapBase& infomap, const OutputArtifact& output)
  {
    switch (output.kind) {
    case OutputKind::Tree:
      infomap.writeTree(output.filename, output.states);
      break;
    case OutputKind::FlowTree:
      infomap.writeFlowTree(output.filename, output.states);
      break;
    case OutputKind::Newick:
      infomap.writeNewickTree(output.filename, output.states);
      break;
    case OutputKind::Json:
      infomap.writeJsonTree(output.filename, output.states, output.writeLinks);
      break;
    case OutputKind::Csv:
      infomap.writeCsvTree(output.filename, output.states);
      break;
    case OutputKind::Clu:
      infomap.writeClu(output.filename, output.states, output.cluLevel);
      break;
    default:
      throw std::logic_error("Output artifact is not a modular result");
    }
  }

  void writeNetworkOutput(Network& network, const OutputArtifact& output)
  {
    switch (output.kind) {
    case OutputKind::StateNetwork:
      network.writeStateNetwork(output.filename);
      break;
    case OutputKind::PajekNetwork:
    case OutputKind::FlowNetwork:
      network.writePajekNetwork(output.filename, output.printFlow);
      break;
    default:
      throw std::logic_error("Output artifact is not a network result");
    }
  }

} // namespace

std::string outputPlanBasename(const Config& config, int trial)
{
  std::string basename = config.outDirectory + config.outName;

  if (config.printAllTrials && trial != -1 && config.numTrials > 1) {
    basename += fmt::format(FMT_STRING("_trial_{}"), trial);
  }

  return basename;
}

std::vector<OutputArtifact> planOutputArtifacts(const Config& config, const std::string& basename, OutputPhase phase)
{
  if (config.noFileOutput) {
    return {};
  }

  std::vector<OutputArtifact> artifacts;

  if (phase == OutputPhase::BeforeFlow) {
    if (config.printStateNetwork) {
      artifacts.push_back(artifact(config, basename, phase, OutputKind::StateNetwork, "states", "state network", true));
    }
    if (config.printPajekNetwork) {
      artifacts.push_back(config.printStates()
                              ? artifact(config, basename, phase, OutputKind::PajekNetwork, "states_as_physical", "state network as Pajek")
                              : artifact(config, basename, phase, OutputKind::PajekNetwork, "net", "Pajek network"));
    }
    return artifacts;
  }

  if (phase == OutputPhase::AfterFlow) {
    if (config.printFlowNetwork) {
      artifacts.push_back(config.printStates()
                              ? artifact(config, basename, phase, OutputKind::FlowNetwork, "flow_as_physical", "flow state network as Pajek", true, true)
                              : artifact(config, basename, phase, OutputKind::FlowNetwork, "flow", "flow network", false, true));
    }
    return artifacts;
  }

  if (phase != OutputPhase::AfterPartition) {
    return artifacts;
  }

  if (config.printTree) {
    addPhysicalAndStateArtifacts(artifacts, config, basename, OutputKind::Tree, "tree", "tree_states", "tree", "physical tree", "state tree");
  }
  if (config.printFlowTree) {
    addPhysicalAndStateArtifacts(artifacts, config, basename, OutputKind::FlowTree, "ftree", "ftree_states", "flow tree", "physical flow tree", "state flow tree");
  }
  if (config.printNewick) {
    addPhysicalAndStateArtifacts(artifacts, config, basename, OutputKind::Newick, "newick", "newick_states", "Newick tree", "physical Newick tree", "state Newick tree");
  }
  if (config.printJson) {
    addPhysicalAndStateArtifacts(artifacts, config, basename, OutputKind::Json, "json", "json_states", "JSON tree", "physical JSON tree", "state JSON tree");
  }
  if (config.printCsv) {
    addPhysicalAndStateArtifacts(artifacts, config, basename, OutputKind::Csv, "csv", "csv_states", "CSV tree", "physical CSV tree", "state CSV tree");
  }
  if (config.printClu) {
    addPhysicalAndStateArtifacts(artifacts, config, basename, OutputKind::Clu, "clu", "clu_states", "node modules", "physical node modules", "state node modules");
  }

  return artifacts;
}

std::vector<OutputArtifact> planOutputArtifacts(const Config& config, OutputPhase phase, int trial)
{
  return planOutputArtifacts(config, outputPlanBasename(config, trial), phase);
}

std::vector<std::pair<std::string, std::string>> planReportArtifacts(const Config& config)
{
  std::vector<std::pair<std::string, std::string>> reports;
  const auto add = [&](const std::string& key, const std::string& path) {
    if (!path.empty() && path != "-")
      reports.emplace_back(key, path);
  };
  add("summary_json", config.summaryJsonPath);
  add("timing_json", config.timingJsonPath);
  add("run_manifest", config.runManifestPath);
  return reports;
}

std::vector<std::string> planAllOutputPaths(const Config& config)
{
  std::vector<std::string> paths;

  const auto collectPhase = [&](OutputPhase phase, int trial) {
    for (const auto& artifact : planOutputArtifacts(config, phase, trial))
      paths.push_back(artifact.filename);
  };

  // Network sidecars are written once, independent of trial.
  collectPhase(OutputPhase::BeforeFlow, -1);
  collectPhase(OutputPhase::AfterFlow, -1);

  // The final modular result is written once with the canonical basename, and
  // additionally per trial when --print-all-trials uses separate files.
  collectPhase(OutputPhase::AfterPartition, -1);
  if (config.printAllTrials && config.numTrials > 1) {
    for (unsigned int trial = 1; trial <= config.numTrials; ++trial)
      collectPhase(OutputPhase::AfterPartition, static_cast<int>(trial));
  }

  for (const auto& report : planReportArtifacts(config))
    paths.push_back(report.second);

  return paths;
}

void preflightOutputTargets(const Config& config)
{
  if (config.overwriteOutput())
    return;

  for (const auto& path : planAllOutputPaths(config)) {
    if (pathExists(path))
      throw InfomapError(ExitCode::OutputError, fmt::format(FMT_STRING("Output file already exists: '{}'"), path));
  }
}

void writeOutputArtifact(InfomapBase& infomap, Network& network, const OutputArtifact& output)
{
  if (output.phase == OutputPhase::AfterPartition) {
    writeModularOutput(infomap, output);
    return;
  }

  writeNetworkOutput(network, output);
}

void writeOutputArtifacts(InfomapBase& infomap, Network& network, OutputPhase phase, int trial)
{
  const auto artifacts = planOutputArtifacts(infomap, phase, trial);
  std::vector<std::pair<std::string, std::string>> prettyOutputFiles;

  for (const auto& output : artifacts) {
    writeOutputArtifact(infomap, network, output);
    if (phase != OutputPhase::AfterPartition) {
      Console().status("Output", fmt::format(FMT_STRING("{} -> {}"), output.label, output.filename));
      continue;
    }
    prettyOutputFiles.emplace_back(output.label, output.filename);
  }

  if (prettyOutputFiles.empty()) {
    return;
  }

  if (prettyOutputFiles.size() == 1) {
    Console().status("Output", fmt::format(FMT_STRING("{} -> {}"), prettyOutputFiles.front().first, prettyOutputFiles.front().second));
    return;
  }

  const auto summaries = summarizeOutputFiles(prettyOutputFiles);
  for (unsigned int i = 0; i < summaries.size(); ++i) {
    const std::string prefix = i == 0 ? fmt::format(FMT_STRING("{} files -> "), prettyOutputFiles.size()) : "         ";
    Console().status("Output", fmt::format(FMT_STRING("{}{}"), prefix, summaries[i]));
  }
}

} // namespace infomap
