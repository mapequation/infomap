/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <map>
#include <string>
#include <utility>

namespace infomap {

class InfomapBase;
class InfoNode;

class Output {
public:
  static std::string writeTree(InfomapBase& im, const std::string& f, bool states);

  static std::string writeFlowTree(InfomapBase& im, const std::string& f, bool states);

  static std::string writeNewickTree(InfomapBase& im, const std::string& f, bool states);

  static std::string writeJsonTree(InfomapBase& im, const std::string& f, bool states, bool writeLinks);

  static std::string writeCsvTree(InfomapBase& im, const std::string& f, bool states);

  static std::string writeClu(InfomapBase& im, const std::string& f, bool states, int moduleIndexLevel);

private:
  static void writeTree(InfomapBase& im, std::ostream& o, bool states);

  static void writeTreeLinks(InfomapBase& im, std::ostream& o, bool states);

  static void writeNewickTree(InfomapBase& im, std::ostream& o, bool states);

  static void writeJsonTree(InfomapBase& im, std::ostream& o, bool states, bool writeLinks);

  static void writeCsvTree(InfomapBase& im, std::ostream& o, bool states);

  static std::string getOutputFileHeader(const InfomapBase& im, bool states = false);

  static std::string getNodeName(const InfomapBase& im, const InfoNode& node);

  using Link = std::pair<unsigned int, unsigned int>;
  using LinkMap = std::map<Link, double>;

  static std::map<std::string, LinkMap> aggregateModuleLinks(InfomapBase& im, bool states = false);
};

} // namespace infomap

#endif // OUTPUT_H_
