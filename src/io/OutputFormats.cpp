/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "OutputFormats.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <sstream>
#include <utility>

namespace infomap {

namespace {

  const std::string textMimeType = "text/plain;charset=utf-8";
  const std::string jsonMimeType = "application/json;charset=utf-8";

  OutputFileFormat file(const std::string& resultKey, const std::string& displayName, bool states, const std::string& suffix, const std::string& extension, const std::string& mimeType, unsigned int resultOrder)
  {
    return { resultKey, displayName, states, suffix, extension, mimeType, resultOrder };
  }

  OutputFormat format(const std::string& optionName, OutputKind kind, std::vector<OutputFileFormat> files)
  {
    return { optionName, kind, std::move(files) };
  }

  std::string jsonString(const std::string& value)
  {
    std::ostringstream escaped;
    escaped << '"';
    for (auto c : value) {
      if (c == '"' || c == '\\') {
        escaped << '\\';
      }
      escaped << c;
    }
    escaped << '"';
    return escaped.str();
  }

  std::vector<OutputFileFormat> orderedOutputFiles()
  {
    std::vector<OutputFileFormat> files;
    for (const auto& format : outputFormats()) {
      for (const auto& file : format.files) {
        files.push_back(file);
      }
    }
    std::sort(files.begin(), files.end(), [](const OutputFileFormat& a, const OutputFileFormat& b) {
      return a.resultOrder < b.resultOrder;
    });
    return files;
  }

} // namespace

const std::vector<OutputFormat>& outputFormats()
{
  static const std::vector<OutputFormat> formats = {
    format("clu", OutputKind::Clu, {
                                       file("clu", "Clu", false, "", "clu", textMimeType, 0),
                                       file("clu_states", "Clu", true, "_states", "clu", textMimeType, 5),
                                   }),
    format("tree", OutputKind::Tree, {
                                         file("tree", "Tree", false, "", "tree", textMimeType, 1),
                                         file("tree_states", "Tree", true, "_states", "tree", textMimeType, 6),
                                     }),
    format("ftree", OutputKind::FlowTree, {
                                              file("ftree", "Ftree", false, "", "ftree", textMimeType, 2),
                                              file("ftree_states", "Ftree", true, "_states", "ftree", textMimeType, 7),
                                          }),
    format("newick", OutputKind::Newick, {
                                             file("newick", "Newick", false, "", "nwk", textMimeType, 11),
                                             file("newick_states", "Newick", true, "_states", "nwk", textMimeType, 12),
                                         }),
    format("json", OutputKind::Json, {
                                         file("json", "JSON", false, "", "json", jsonMimeType, 13),
                                         file("json_states", "JSON", true, "_states", "json", jsonMimeType, 14),
                                     }),
    format("csv", OutputKind::Csv, {
                                       file("csv", "CSV", false, "", "csv", textMimeType, 15),
                                       file("csv_states", "CSV", true, "_states", "csv", textMimeType, 16),
                                   }),
    format("network", OutputKind::PajekNetwork, {
                                                    file("net", "Network", false, "", "net", textMimeType, 3),
                                                    file("states_as_physical", "States as physical", false, "_states_as_physical", "net", textMimeType, 4),
                                                }),
    format("states", OutputKind::StateNetwork, {
                                                   file("states", "States", true, "_states", "net", textMimeType, 8),
                                               }),
    format("flow", OutputKind::FlowNetwork, {
                                                file("flow", "Flow", false, "_flow", "net", textMimeType, 9),
                                                file("flow_as_physical", "Flow states as physical", true, "_states_as_physical_flow", "net", textMimeType, 10),
                                            }),
  };
  return formats;
}

std::vector<std::string> outputFormatNames()
{
  const auto& formats = outputFormats();
  std::vector<std::string> names;
  names.reserve(formats.size());
  std::transform(formats.begin(), formats.end(), std::back_inserter(names),
      [](const auto& format) { return format.optionName; });
  return names;
}

const OutputFormat* findOutputFormat(const std::string& optionName)
{
  for (const auto& format : outputFormats()) {
    if (format.optionName == optionName)
      return &format;
  }
  return nullptr;
}

const OutputFileFormat* findOutputFileFormat(const std::string& resultKey)
{
  for (const auto& format : outputFormats()) {
    for (const auto& file : format.files) {
      if (file.resultKey == resultKey)
        return &file;
    }
  }
  return nullptr;
}

std::string outputFilename(const std::string& basename, const OutputFileFormat& file)
{
  return basename + file.suffix + "." + file.extension;
}

std::string outputFilenameForResultKey(const std::string& basename, const std::string& resultKey)
{
  const auto* file = findOutputFileFormat(resultKey);
  if (file == nullptr)
    throw std::logic_error("Unknown output result key: " + resultKey);
  return outputFilename(basename, *file);
}

std::string outputFormatsManifestJson()
{
  std::ostringstream json;
  json << "{\n"
       << "  \"resultOrder\": [\n";
  bool firstResultKey = true;
  for (const auto& file : orderedOutputFiles()) {
    if (!firstResultKey) {
      json << ",\n";
    }
    firstResultKey = false;
    json << "    " << jsonString(file.resultKey);
  }
  json << "\n"
       << "  ],\n"
       << "  \"formats\": [\n";
  bool firstFormat = true;
  for (const auto& format : outputFormats()) {
    if (!firstFormat) {
      json << ",\n";
    }
    firstFormat = false;
    json << "    {\n"
         << "      \"optionName\": " << jsonString(format.optionName) << ",\n"
         << "      \"files\": [\n";

    bool firstFile = true;
    for (const auto& file : format.files) {
      if (!firstFile) {
        json << ",\n";
      }
      firstFile = false;
      json << "        {"
           << "\"key\": " << jsonString(file.resultKey) << ", "
           << "\"name\": " << jsonString(file.displayName) << ", "
           << "\"isStates\": " << (file.states ? "true" : "false") << ", "
           << "\"suffix\": " << jsonString(file.suffix) << ", "
           << "\"extension\": " << jsonString(file.extension) << ", "
           << "\"mimeType\": " << jsonString(file.mimeType)
           << "}";
    }

    json << "\n"
         << "      ]\n"
         << "    }";
  }
  json << "\n  ]\n}\n";
  return json.str();
}

} // namespace infomap
