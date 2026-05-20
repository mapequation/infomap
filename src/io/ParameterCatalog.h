/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef PARAMETER_CATALOG_H_
#define PARAMETER_CATALOG_H_

#include <cstdint>
#include <string>
#include <vector>

namespace infomap {

struct Config;
class ProgramInterface;

enum class ParameterId : std::uint8_t {
  Help,
  Version,
  Completion,
  NetworkFile,
  Input,
  SkipAdjustBipartiteFlow,
  BipartiteTeleportation,
  WeightThreshold,
  IncludeSelfLinks,
  NoSelfLinks,
  NodeLimit,
  MatchableMultilayerIds,
  ClusterData,
  AssignToNeighbouringModule,
  MetaData,
  MetaDataRate,
  MetaDataUnweighted,
  NoInfomap,
  OutName,
  NoFileOutput,
  Tree,
  FlowTree,
  Clu,
  CluLevel,
  Output,
  HideBipartiteNodes,
  PrintAllTrials,
  TwoLevel,
  FlowModel,
  Directed,
  RecordedTeleportation,
  UseNodeWeightsAsFlow,
  TeleportToNodes,
  TeleportationProbability,
  Regularized,
  RegularizationStrength,
  EntropyCorrected,
  EntropyCorrectionStrength,
  MarkovTime,
  VariableMarkovTime,
  VariableMarkovDamping,
  VariableMarkovMinScale,
  PreferredNumberOfModules,
  MultilayerRelaxRate,
  MultilayerRelaxLimit,
  MultilayerRelaxLimitUp,
  MultilayerRelaxLimitDown,
  MultilayerRelaxByJsd,
  Seed,
  NumTrials,
  CoreLoopLimit,
  CoreLevelLimit,
  TuneIterationLimit,
  CoreLoopCodelengthThreshold,
  TuneIterationRelativeThreshold,
  FastHierarchicalSolution,
  InnerParallelization,
  PreferModularSolution,
  NumRandomMoves,
  MaxDegreeForRandomMoves,
  OutputDirectory,
  Verbose,
  Silent,
  Pretty
};

struct ParameterSpec {
  ParameterId id;
  char shortName;
  std::string longName;
  std::string description;
  std::string argumentName;
  std::string group;
  bool isAdvanced = false;
  bool hidden = false;
  bool requireArgument = false;
  bool incrementalArgument = false;
  std::string defaultValue;
  std::string minValue;
  std::string maxValue;
  std::vector<std::string> choices;
  std::string pythonName;
  std::string rName;
  std::string tsName;
  std::string renderPolicy;
  std::string pythonDefault;
  std::string rDefault;
  std::string pythonDocDescription;
};

struct ConfigParameterTargets {
  Config& config;
  std::string& flowModelArg;
  bool& deprecatedIncludeSelfLinks;
  std::vector<std::string>& optionalOutputDir;
};

// Declarative source of truth for Infomap parameter metadata. Config remains the
// mutable runtime state that receives parsed values and applies adaptation rules.
const std::vector<ParameterSpec>& parameterCatalog();
void registerConfigParameters(ProgramInterface& api, ConfigParameterTargets targets, bool isCli);
std::string parameterCatalogJson();

} // namespace infomap

#endif // PARAMETER_CATALOG_H_
