/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ConfigBuilder.h"
#include "Config.h"
#include "ProgramInterface.h"
#include "SafeFile.h"
#include "../utils/FileURI.h"
#include "../utils/Log.h"
#include "../utils/convert.h"

#include <stdexcept>

namespace infomap {

namespace {

  void applyLibraryOutputDefaults(Config& config, bool isCLI)
  {
    if (!isCLI && config.outDirectory.empty())
      config.noFileOutput = true;
  }

  void validateRequiredCliOutput(const Config& config, bool isCLI)
  {
    if (!config.noFileOutput && config.outDirectory.empty() && isCLI) {
      throw std::runtime_error("Missing out_directory");
    }
  }

  void applyOptionInteractions(Config& config)
  {
    if (config.regularized) {
      config.recordedTeleportation = true;
    }

    if (config.noInfomap) {
      config.numTrials = 1;
    }
  }

  void applyRuntimeOutputInteractions(Config& config)
  {
    if (config.verbosity > 0) {
      config.prettyOutput = false;
    }

    if (config.printAllTrials && config.numTrials < 2) {
      config.printAllTrials = false;
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

  void applyOutputNameDefault(Config& config)
  {
    if (config.outName.empty()) {
      config.outName = !config.networkFile.empty() ? FileURI(config.networkFile).getName() : "no-name";
    }
  }

  void initializeLogging(const Config& config)
  {
    Log::init(config.verbosity, config.silent, config.verboseNumberPrecision, config.prettyOutput);
  }

} // namespace

void ConfigBuilder::buildFromFlags(Config& config, const std::string& flags, bool isCLI)
{
  config = Config {};
  config.isCLI = isCLI;
  config.parsedString = flags;

  const auto parsed = parseRaw(config, flags, isCLI);
  config.parsedOptions = parsed.usedOptions;
  applyParsed(config, parsed, isCLI);

  initializeLogging(config);
}

ParsedConfigParameters ConfigBuilder::parseRaw(Config& config, const std::string& flags, bool isCLI)
{
  ProgramInterface api("Infomap",
                       "Implementation of the Infomap clustering algorithm based on the Map Equation (see www.mapequation.org)",
                       INFOMAP_VERSION);

  api.setGroups({ "Input", "Algorithm", "Accuracy", "Output" });
  api.setJsonParametersProvider(parameterCatalogJson);

  ParsedConfigParameters parsed;
  registerCatalogWithProgramInterface(api, { config, parsed }, isCLI);

  api.parseArgs(flags);
  parsed.usedOptions = api.getUsedOptionArguments();
  return parsed;
}

void ConfigBuilder::applyParsed(Config& config, const ParsedConfigParameters& parsed, bool isCLI)
{
  config.isCLI = isCLI;
  applyParsedParameters(config, parsed);
  applyLibraryOutputDefaults(config, isCLI);
  validateRequiredCliOutput(config, isCLI);
  applyOptionInteractions(config);
  normalizeOutputDirectory(config);
  validateOutputDirectory(config);
  applyOutputNameDefault(config);
  applyRuntimeOutputInteractions(config);
  config.adaptDefaults();
}

} // namespace infomap
