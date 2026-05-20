/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef OUTPUT_PLAN_H_
#define OUTPUT_PLAN_H_

#include "Config.h"
#include "OutputFormats.h"

#include <string>
#include <vector>

namespace infomap {

class InfomapBase;
class Network;

enum class OutputPhase : std::uint8_t {
  BeforeFlow,
  AfterFlow,
  AfterPartition
};

struct OutputArtifact {
  std::string resultKey;
  std::string label;
  std::string filename;
  OutputPhase phase = OutputPhase::AfterPartition;
  OutputKind kind = OutputKind::Tree;
  bool states = false;
  int cluLevel = 1;
  bool writeLinks = false;
  bool printFlow = false;
};

std::string outputPlanBasename(const Config& config, int trial = -1);

std::vector<OutputArtifact> planOutputArtifacts(const Config& config, const std::string& basename, OutputPhase phase);

std::vector<OutputArtifact> planOutputArtifacts(const Config& config, OutputPhase phase, int trial = -1);

void writeOutputArtifact(InfomapBase& infomap, Network& network, const OutputArtifact& artifact);

void writeOutputArtifacts(InfomapBase& infomap, Network& network, OutputPhase phase, int trial = -1);

} // namespace infomap

#endif // OUTPUT_PLAN_H_
