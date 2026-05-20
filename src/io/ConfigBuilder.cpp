/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ConfigBuilder.h"
#include "Config.h"
#include "ParameterCatalog.h"
#include "ProgramInterface.h"
#include "SafeFile.h"
#include "../utils/FileURI.h"
#include "../utils/convert.h"

#include <stdexcept>

namespace infomap {

namespace {

  void applyFlowModelSelection(Config& config, const std::string& flowModelArg)
  {
    if (flowModelArg == "directed" || config.directed) {
      config.setFlowModel(FlowModel::directed);
    } else if (flowModelArg == "undirected") {
      config.setFlowModel(FlowModel::undirected);
    } else if (flowModelArg == "undirdir") {
      config.setFlowModel(FlowModel::undirdir);
    } else if (flowModelArg == "outdirdir") {
      config.setFlowModel(FlowModel::outdirdir);
    } else if (flowModelArg == "rawdir") {
      config.setFlowModel(FlowModel::rawdir);
    } else if (flowModelArg == "precomputed") {
      config.setFlowModel(FlowModel::precomputed);
    } else if (!flowModelArg.empty()) {
      throw std::runtime_error(io::Str() << "Unrecognized flow model: '" << flowModelArg << "'");
    }
  }

  void normalizeOutputDirectory(Config& config)
  {
    if (!config.haveOutput() || config.outDirectory.empty())
      return;

    if (config.outDirectory.back() != '/')
      config.outDirectory.push_back('/');
  }

  void validateOutputDirectory(const Config& config)
  {
    if (config.haveOutput() && !isDirectoryWritable(config.outDirectory))
      throw std::runtime_error(io::Str() << "Can't write to directory '" << config.outDirectory << "'. Check that the directory exists and that you have write permissions.");
  }

} // namespace

ParsedConfigParameters ConfigBuilder::parseRaw(Config& config, const std::string& flags, bool isCLI)
{
  ProgramInterface api("Infomap",
                       "Implementation of the Infomap clustering algorithm based on the Map Equation (see www.mapequation.org)",
                       INFOMAP_VERSION);

  api.setGroups({ "Input", "Algorithm", "Accuracy", "Output" });
  api.setJsonParametersProvider(parameterCatalogJson);

  ParsedConfigParameters parsed;
  registerConfigParameters(api, { config, parsed.flowModelArg, parsed.deprecatedIncludeSelfLinks, parsed.optionalOutputDir }, isCLI);

  api.parseArgs(flags);
  parsed.usedOptions = api.getUsedOptionArguments();
  return parsed;
}

void ConfigBuilder::applyParsed(Config& config, const ParsedConfigParameters& parsed, bool isCLI)
{
  if (parsed.deprecatedIncludeSelfLinks) {
    throw std::runtime_error("The --include-self-links flag is deprecated; self-links are included by default. Use --no-self-links to exclude them.");
  }

  if (!parsed.optionalOutputDir.empty())
    config.outDirectory = parsed.optionalOutputDir[0];

  if (!isCLI && config.outDirectory.empty())
    config.noFileOutput = true;

  if (!config.noFileOutput && config.outDirectory.empty() && isCLI) {
    throw std::runtime_error("Missing out_directory");
  }

  applyFlowModelSelection(config, parsed.flowModelArg);

  if (config.regularized) {
    config.recordedTeleportation = true;
  }

  normalizeOutputDirectory(config);
  validateOutputDirectory(config);

  if (config.outName.empty()) {
    config.outName = !config.networkFile.empty() ? FileURI(config.networkFile).getName() : "no-name";
  }

  if (config.noInfomap) {
    config.numTrials = 1;
  }
}

} // namespace infomap
