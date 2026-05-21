/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef PARAMETER_CATALOG_H_
#define PARAMETER_CATALOG_H_

#include "ParsedOption.h"

#include <functional>
#include <string>
#include <vector>

namespace infomap {

struct Config;
class ProgramInterface;

struct ParsedParameterSet {
  std::string flowModelArg;
  bool deprecatedIncludeSelfLinks = false;
  std::vector<std::string> optionalOutputDir;
  std::vector<ParsedOption> usedOptions;
};

struct ConfigParameterTargets {
  Config& config;
  ParsedParameterSet& parsed;
};

struct ParameterSpec {
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
  bool cliOnly = false;
  bool libraryOnly = false;
  bool includeInJson = true;
  std::function<void(ProgramInterface&, ConfigParameterTargets&, const ParameterSpec&)> registrar;
};

// Declarative source of truth for Infomap parameter metadata. Config remains the
// mutable runtime state that receives parsed values and applies adaptation rules.
const std::vector<ParameterSpec>& parameterCatalog();
void registerCatalogWithProgramInterface(ProgramInterface& api, ConfigParameterTargets targets, bool isCli);
void registerConfigParameters(ProgramInterface& api, ConfigParameterTargets targets, bool isCli);
void applyParsedParameters(Config& config, const ParsedParameterSet& parsed);
std::string parameterCatalogJson();

} // namespace infomap

#endif // PARAMETER_CATALOG_H_
