/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "OutputPlan.h"
#include "Network.h"
#include "OutputFormats.h"
#include "../core/InfomapBase.h"
#include "../utils/Log.h"
#include "../utils/PrettyOutput.h"
#include "../utils/convert.h"

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
        summaries.push_back(group.second.front().empty() ? group.first : io::Str() << group.first << "." << group.second.front());
        continue;
      }
      summaries.push_back(io::Str() << group.first << ".{" << io::stringify(group.second, ",") << "}");
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
  io::Str basename;
  basename << config.outDirectory + config.outName;

  if (config.printAllTrials && trial != -1 && config.numTrials > 1) {
    basename << "_trial_" << trial;
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

void writeOutputArtifact(InfomapBase& infomap, Network& network, const OutputArtifact& output)
{
  if (output.phase == OutputPhase::AfterPartition) {
    const auto write = [&]() { writeModularOutput(infomap, output); };
    if (infomap.prettyOutput) {
      write();
      return;
    }

    Log() << "Write " << output.label << " to " << output.filename << "... ";
    write();
    Log() << "done!\n";
    return;
  }

  Log() << "Writing " << output.label << " to '" << output.filename << "'... ";
  writeNetworkOutput(network, output);
  Log() << "done!\n";
}

void writeOutputArtifacts(InfomapBase& infomap, Network& network, OutputPhase phase, int trial)
{
  const auto artifacts = planOutputArtifacts(infomap, phase, trial);
  std::vector<std::pair<std::string, std::string>> prettyOutputFiles;

  for (const auto& output : artifacts) {
    writeOutputArtifact(infomap, network, output);
    if (infomap.prettyOutput) {
      if (phase != OutputPhase::AfterPartition) {
        PrettyOutput(true).status("Output", io::Str() << output.label << " -> " << output.filename);
        continue;
      }
      prettyOutputFiles.emplace_back(output.label, output.filename);
    }
  }

  if (!infomap.prettyOutput || prettyOutputFiles.empty()) {
    return;
  }

  if (prettyOutputFiles.size() == 1) {
    PrettyOutput(true).status("Output", io::Str() << prettyOutputFiles.front().first << " -> " << prettyOutputFiles.front().second);
    return;
  }

  const auto summaries = summarizeOutputFiles(prettyOutputFiles);
  for (unsigned int i = 0; i < summaries.size(); ++i) {
    const std::string prefix = i == 0 ? std::string(io::Str() << prettyOutputFiles.size() << " files -> ") : "         ";
    PrettyOutput(true).status("Output", io::Str() << prefix << summaries[i]);
  }
}

} // namespace infomap
