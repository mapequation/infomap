/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "OutputFormats.h"

#include <sstream>
#include <utility>

namespace infomap {

namespace {

  const std::string textMimeType = "text/plain;charset=utf-8";
  const std::string jsonMimeType = "application/json;charset=utf-8";

  OutputFileFormat file(const std::string& resultKey, const std::string& displayName, bool states, const std::string& suffix, const std::string& extension, const std::string& mimeType)
  {
    return { resultKey, displayName, states, suffix, extension, mimeType };
  }

  OutputFormat format(const std::string& optionName, OutputFormatFlag flag, std::vector<OutputFileFormat> files)
  {
    return { optionName, flag, std::move(files) };
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

} // namespace

const std::vector<OutputFormat>& outputFormats()
{
  static const std::vector<OutputFormat> formats = {
    format("clu", OutputFormatFlag::Clu, {
                                             file("clu", "Clu", false, "", "clu", textMimeType),
                                             file("clu_states", "Clu", true, "_states", "clu", textMimeType),
                                         }),
    format("tree", OutputFormatFlag::Tree, {
                                               file("tree", "Tree", false, "", "tree", textMimeType),
                                               file("tree_states", "Tree", true, "_states", "tree", textMimeType),
                                           }),
    format("ftree", OutputFormatFlag::FlowTree, {
                                                    file("ftree", "Ftree", false, "", "ftree", textMimeType),
                                                    file("ftree_states", "Ftree", true, "_states", "ftree", textMimeType),
                                                }),
    format("newick", OutputFormatFlag::Newick, {
                                                   file("newick", "Newick", false, "", "nwk", textMimeType),
                                                   file("newick_states", "Newick", true, "_states", "nwk", textMimeType),
                                               }),
    format("json", OutputFormatFlag::Json, {
                                               file("json", "JSON", false, "", "json", jsonMimeType),
                                               file("json_states", "JSON", true, "_states", "json", jsonMimeType),
                                           }),
    format("csv", OutputFormatFlag::Csv, {
                                             file("csv", "CSV", false, "", "csv", textMimeType),
                                             file("csv_states", "CSV", true, "_states", "csv", textMimeType),
                                         }),
    format("network", OutputFormatFlag::PajekNetwork, {
                                                          file("net", "Network", false, "", "net", textMimeType),
                                                          file("states_as_physical", "States as physical", false, "_states_as_physical", "net", textMimeType),
                                                      }),
    format("states", OutputFormatFlag::StateNetwork, {
                                                         file("states", "States", true, "_states", "net", textMimeType),
                                                     }),
    format("flow", OutputFormatFlag::FlowNetwork, {
                                                      file("flow", "Flow", false, "_flow", "net", textMimeType),
                                                      file("flow_as_physical", "Flow states as physical", true, "_states_as_physical_flow", "net", textMimeType),
                                                  }),
  };
  return formats;
}

std::vector<std::string> outputFormatNames()
{
  std::vector<std::string> names;
  for (const auto& format : outputFormats()) {
    names.push_back(format.optionName);
  }
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

std::string outputFilename(const std::string& basename, const OutputFileFormat& file)
{
  return basename + file.suffix + "." + file.extension;
}

std::string outputFormatsManifestJson()
{
  std::ostringstream json;
  json << "{\n"
       << "  \"resultOrder\": [\n"
       << "    \"clu\",\n"
       << "    \"tree\",\n"
       << "    \"ftree\",\n"
       << "    \"net\",\n"
       << "    \"states_as_physical\",\n"
       << "    \"clu_states\",\n"
       << "    \"tree_states\",\n"
       << "    \"ftree_states\",\n"
       << "    \"states\",\n"
       << "    \"flow\",\n"
       << "    \"flow_as_physical\",\n"
       << "    \"newick\",\n"
       << "    \"newick_states\",\n"
       << "    \"json\",\n"
       << "    \"json_states\",\n"
       << "    \"csv\",\n"
       << "    \"csv_states\"\n"
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
