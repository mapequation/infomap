/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef CONFIG_BUILDER_H_
#define CONFIG_BUILDER_H_

#include "ParameterCatalog.h"

#include <string>

namespace infomap {

struct Config;

using ParsedConfigParameters = ParsedParameterSet;

class ConfigBuilder {
public:
  static void buildFromFlags(Config& config, const std::string& flags, bool isCLI);
  static ParsedConfigParameters parseRaw(Config& config, const std::string& flags, bool isCLI);
  static void applyParsed(Config& config, const ParsedConfigParameters& parsed, bool isCLI);
};

} // namespace infomap

#endif // CONFIG_BUILDER_H_
