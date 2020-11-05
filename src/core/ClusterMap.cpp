#include <sstream>
#include "ClusterMap.h"
#include "../io/SafeFile.h"
#include "../io/convert.h"
#include "../utils/Log.h"
#include "../utils/FileURI.h"

namespace infomap {

void ClusterMap::readClusterData(const std::string& filename, bool includeFlow)
{
  FileURI file(filename);
  m_extension = file.getExtension();
  if (m_extension == "tree" || m_extension == "ftree") {
    return readTree(filename, includeFlow);
  }
  if (m_extension == "clu") {
    return readClu(filename, includeFlow);
  }
  throw ImplementationError(io::Str() << "Input cluster data from file '" << filename << "' is of unknown extension '" << m_extension << "'. Must be 'clu' or 'tree'.");
}

/**
 * Sample from .tree file
# Codelength = 3.46227314 bits.
# path flow name physicalId
1:1:1 0.0384615 "1" 1
1:1:2 0.025641 "2" 2
1:1:3 0.0384615 "3" 3
1:2:1 0.0384615 "4" 4
 */
void ClusterMap::readTree(const std::string& filename, bool includeFlow)
{
  SafeInFile input(filename);
  std::string line;
  std::istringstream lineStream;
  std::istringstream pathStream;
  m_nodePaths.clear();

  std::string header;
  unsigned int lineNr = 0;
  std::string section;

  while (!std::getline(input, line).fail()) {
    // if (line.length() == 0 || line[0] == '#' || line[0] == '*')
    // 	continue;
    ++lineNr;
    if (line.length() == 0)
      continue;
    if (line[0] == '#') {
      if (lineNr == 1) {
        header = line; // e.g. '# Codelength = 8.45977 bits.'
      }
      continue;
    }
    if (line[0] == '*') {
      // New section, abort tree parsing.
      section = line;
      break;
    }

    lineStream.clear();
    lineStream.str(line);

    std::string pathString;
    double flow;
    std::string name;
    unsigned int nodeId;
    unsigned int physId;
    // if (!(lineStream >> pathString >> flow >> name >> nodeId))
    // 	throw FileFormatError(io::Str() << "Couldn't parse .tree line '" << line << "'");
    if (!(lineStream >> pathString))
      throw FileFormatError(io::Str() << "Couldn't parse tree path from line '" << line << "'");
    if (!(lineStream >> flow))
      throw FileFormatError(io::Str() << "Couldn't parse node flow from line '" << line << "'");
    // Get the name by extracting the rest of the stream until the first quotation mark and then the last.
    if (!getline(lineStream, name, '"'))
      throw BadConversionError(io::Str() << "Can't parse node name from line " << lineNr << " ('" << line << "').");
    if (!getline(lineStream, name, '"'))
      throw BadConversionError(io::Str() << "Can't parse node name from line " << lineNr << " ('" << line << "').");
    if (!(lineStream >> nodeId))
      throw FileFormatError(io::Str() << "Couldn't parse node id from line '" << line << "'");
    if (lineStream >> physId) {
      m_isHigherOrder = true;
    } else if (m_isHigherOrder) {
      throw FileFormatError(io::Str() << "Missing stateId for node on line '" << line << "'.");
    }

    pathStream.clear();
    pathStream.str(pathString);
    unsigned int childNumber;
    Path path;
    while (pathStream >> childNumber) {
      pathStream.get(); // Extract the delimiting character also
      if (childNumber == 0)
        throw FileFormatError("There is a '0' in the tree path, lowest allowed integer is 1.");
      path.push_back(childNumber); // Keep 1-based indexing in path
    }
    m_nodePaths.emplace_back(nodeId, path);
  }
}

void ClusterMap::readClu(const std::string& filename, bool includeFlow)
{
  Log() << "Read initial partition from '" << filename << "'... " << std::flush;
  SafeInFile input(filename);
  std::string line;
  std::istringstream lineStream;
  std::map<unsigned int, unsigned int> clusterData;

  while (!std::getline(input, line).fail()) {
    if (line.length() == 0 || line[0] == '#' || line[0] == '*')
      continue;

    lineStream.clear();
    lineStream.str(line);

    unsigned int nodeId;
    unsigned int clusterId;
    if (!(lineStream >> nodeId >> clusterId))
      throw FileFormatError(io::Str() << "Couldn't parse node key and cluster id from line '" << line << "'");
    m_clusterIds[nodeId] = clusterId;

    auto flow = 0.0;
    if (includeFlow && lineStream >> flow) {
      m_flowData[nodeId] = flow;
    }
  }
}

} // namespace infomap
