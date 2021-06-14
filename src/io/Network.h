/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

 For more information, see <http://www.mapequation.org>


 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/


#ifndef NETWORK_H_
#define NETWORK_H_

#include <string>
#include <map>
#include <utility>
#include <vector>
#include <set>
#include <utility>
#include "Config.h"
#include <limits>
#include <sstream>
#include "../core/StateNetwork.h"
#include <locale>

namespace infomap {

struct LayerNode;
struct Bigram;
struct Weight;
struct BipartiteLink;

class Network : public StateNetwork {
protected:
  // Helpers
  std::istringstream m_extractor;

  // Multilayer
  std::map<unsigned int, Network> m_networks; // intra-layer links
  std::map<LayerNode, std::map<unsigned int, double>> m_interLinks;
  // std::map<LayerNode, unsigned int> m_layerNodeToStateId;
  // { layer -> { physId -> stateId }}
  std::map<unsigned int, std::map<unsigned int, unsigned int>> m_layerNodeToStateId;
  // std::map<LayerNode, double> m_sumIntraOutWeight;
  std::map<unsigned int, std::map<unsigned int, double>> m_sumIntraOutWeight;
  //asdf m_sumIntraOutWeight
  std::set<unsigned int> m_layers;
  unsigned int m_numInterLayerLinks = 0;
  unsigned int m_numIntraLayerLinks = 0;
  unsigned int m_maxNodeIdInIntraLayerNetworks = 0;

  // Meta data
  std::map<unsigned int, std::vector<int>> m_metaData;
  unsigned int m_numMetaDataColumns = 0;

  using InsensitiveStringSet = std::set<std::string, io::InsensitiveCompare>;

  std::map<std::string, InsensitiveStringSet> m_ignoreHeadings;
  std::map<std::string, InsensitiveStringSet> m_validHeadings; // {
  // 	{ "pajek", {"*Vertices", "*Edges", "*Arcs"} },
  // 	{ "link-list", {"*Links"} },
  // 	{ "bipartite", {"*Vertices", "*Bipartite"} },
  // 	{ "general", {"*Vertices", "*States", "*Edges", "*Arcs", "*Links", "*Context"} }
  // };

public:
  Network() : StateNetwork() { initValidHeadings(); }
  explicit Network(const Config& config) : StateNetwork(config) { initValidHeadings(); }
  explicit Network(std::string flags) : StateNetwork(Config(std::move(flags))) { initValidHeadings(); }
  virtual ~Network() = default;

  virtual void clear();

  /**
    * Parse network data from file and generate network
    * @param filename input network
    * @param accumulate add to possibly existing network data (default), else clear before.
    */
  virtual void readInputData(std::string filename = "", bool accumulate = true);

  /**
   * Init categorical meta data on all nodes from a file with the following format:
   * # nodeId metaData
   * 1 1
   * 2 1
   * 3 2
   * 4 2
   * 5 3
   * @param filename input filename for metadata
   */
  virtual void readMetaData(std::string filename);


  unsigned int numMetaDataColumns() const { return m_numMetaDataColumns; }
  const std::map<unsigned int, std::vector<int>>& metaData() const { return m_metaData; }

  bool isMultilayerNetwork() const { return !m_layerNodeToStateId.empty(); }

  // void printParsingResult(bool onlySummary = false);

  // std::string getParsingResultSummary();

  void generateStateNetworkFromMultilayer();
  void generateStateNetworkFromMultilayerWithInterLinks();
  void generateStateNetworkFromMultilayerWithSimulatedInterLinks();
  void simulateInterLayerLinks();

  void addMultilayerLink(unsigned int layer1, unsigned int n1, unsigned int layer2, unsigned int n2, double weight);

  /**
   * Create an intra-layer link
   */
  void addMultilayerIntraLink(unsigned int layer, unsigned int n1, unsigned int n2, double weight);

  /**
   * Create links between (layer1,n) and (layer2,m) for all m connected to n in layer 2.
   * The weight is distributed proportionally.
   * TODO: This is done later..
   */
  void addMultilayerInterLink(unsigned int layer1, unsigned int n, unsigned int layer2, double interWeight);

  void addMetaData(unsigned int nodeId, int meta);

  void addMetaData(unsigned int nodeId, const std::vector<int>& metaData);

protected:
  void initValidHeadings();

  void parsePajekNetwork(std::string filename);
  void parseLinkList(std::string filename);
  void parseStateNetwork(std::string filename);
  void parseNetwork(std::string filename);
  void parseNetwork(std::string filename, const InsensitiveStringSet& validHeadings, const InsensitiveStringSet& ignoreHeadings, std::string startHeading = "");

  /**
   * Parse a bipartite network with the following format
# A bipartite network with node names
*Vertices 5
1 "Node 1"
2 "Node 2"
3 "Node 3"
4 "Feature 1"
5 "Feature 2"
*Bipartite 4
# set bipartite start id in heading above
4 1
4 2
5 2
5 3
   */
  void parseBipartiteNetwork(std::string filename);
  void parseMultilayerNetwork(std::string filename);

  // Helper methods

  /**
   * Parse vertices under the heading
   * @return The line after the vertices
   */
  std::string parseVertices(std::ifstream& file, std::string heading);
  std::string parseStateNodes(std::ifstream& file, std::string heading);

  std::string parseLinks(std::ifstream& file);

  /**
   * Parse multilayer links from a *multilayer section
   */
  std::string parseMultilayerLinks(std::ifstream& file);

  /**
   * Parse multilayer links from an *intra section
   */
  std::string parseMultilayerIntraLinks(std::ifstream& file);

  /**
   * Parse multilayer links from an *inter section
   */
  std::string parseMultilayerInterLinks(std::ifstream& file);

  std::string parseBipartiteLinks(std::ifstream& file, std::string heading);

  std::string ignoreSection(std::ifstream& file, std::string heading);


  void parseStateNode(const std::string& line, StateNetwork::StateNode& stateNode);

  /**
   * Parse a string of link data.
   * If no weight data can be extracted, the default value 1.0 will be used.
   * @throws an error if not both node ids can be extracted.
   */
  void parseLink(const std::string& line, unsigned int& n1, unsigned int& n2, double& weight);

  /**
   * Parse a string of multilayer link data.
   * If no weight data can be extracted, the default value 1.0 will be used.
   * @throws an error if not both node and layer ids can be extracted.
   */
  void parseMultilayerLink(const std::string& line, unsigned int& layer1, unsigned int& n1, unsigned int& layer2, unsigned int& n2, double& weight);

  /**
   * Parse a string of intra-multilayer link data.
   * If no weight data can be extracted, the default value 1.0 will be used.
   * @throws an error if not both node and layer ids can be extracted.
   */
  void parseMultilayerIntraLink(const std::string& line, unsigned int& layer, unsigned int& n1, unsigned int& n2, double& weight);

  /**
   * Parse a string of inter-multilayer link data.
   * If no weight data can be extracted, the default value 1.0 will be used.
   * @throws an error if not both node and layer ids can be extracted.
   */
  void parseMultilayerInterLink(const std::string& line, unsigned int& layer1, unsigned int& n, unsigned int& layer2, double& weight);

  /**
   * Create state node corresponding to this multilayer node if not already exist
   * @return state node id
   */
  unsigned int addMultilayerNode(unsigned int layerId, unsigned int physicalId, double weight = 1.0);

  double calculateJensenShannonDivergence(bool& intersect, const OutLinkMap& layer1OutLinks, double sumOutLinkWeightLayer1, const OutLinkMap& layer2OutLinks, double sumOutLinkWeightLayer2);

  void printSummary();
};

struct LayerNode {
  unsigned int layer, node;
  explicit LayerNode(unsigned int layer = 0, unsigned int node = 0) : layer(layer), node(node) {}

  bool operator<(const LayerNode other) const
  {
    return layer == other.layer ? node < other.node : layer < other.layer;
  }
};

struct Bigram {
  unsigned int first, second;
  explicit Bigram(unsigned int first = 0, unsigned int second = 0) : first(first), second(second) {}

  bool operator<(const Bigram other) const
  {
    return first == other.first ? second < other.second : first < other.first;
  }
};

struct BipartiteLink {
  unsigned int featureNode, node;
  bool swapOrder;
  explicit BipartiteLink(unsigned int featureNode = 0, unsigned int node = 0, bool swapOrder = false)
      : featureNode(featureNode), node(node), swapOrder(swapOrder) {}

  bool operator<(const BipartiteLink other) const
  {
    return featureNode == other.featureNode ? node < other.node : featureNode < other.featureNode;
  }
};

// Struct to make the weight initialized to zero by default in a map
struct Weight {
  double weight;
  explicit Weight(double weight = 0) : weight(weight) {}

  Weight& operator+=(double w)
  {
    weight += w;
    return *this;
  }
};

template <typename key_t, typename subkey_t, typename value_t>
class MapMap {
public:
  typedef std::map<subkey_t, value_t> submap_t;
  typedef std::map<key_t, submap_t> map_t;
  MapMap() : m_size(0), m_numAggregations(0), m_sumValue(0) {}
  virtual ~MapMap() = default;

  bool insert(key_t key1, subkey_t key2, value_t value)
  {
    ++m_size;
    m_sumValue += value;

    // Aggregate link weights if they are definied more than once
    typename map_t::iterator firstIt = m_data.lower_bound(key1);
    if (firstIt != m_data.end() && firstIt->first == key1) {
      std::pair<typename submap_t::iterator, bool> ret2 = firstIt->second.insert(std::make_pair(key2, value));
      if (!ret2.second) {
        ret2.first->second += value;
        ++m_numAggregations;
        --m_size;
        return false;
      }
    } else {
      m_data.insert(firstIt, std::make_pair(key1, submap_t()))->second.insert(std::make_pair(key2, value));
    }

    return true;
  }

  unsigned int size() const { return m_size; }
  unsigned int numAggregations() const { return m_numAggregations; }
  value_t sumValue() const { return m_sumValue; }
  map_t& data() { return m_data; }
  const map_t& data() const { return m_data; }


private:
  map_t m_data;
  unsigned int m_size;
  unsigned int m_numAggregations;
  value_t m_sumValue;
};

typedef MapMap<unsigned int, unsigned int, double> LinkMapMap;

template <typename key_t, typename value_t>
class EasyMap : public std::map<key_t, value_t> {
public:
  typedef std::map<key_t, value_t> map_t;
  typedef EasyMap<key_t, value_t> self_t;
  value_t& getOrSet(const key_t& key, value_t defaultValue = 0)
  {
    typename self_t::iterator it = lower_bound(key);
    if (it != this->end() && it->first == key)
      return it->second;
    return this->insert(it, std::make_pair(key, defaultValue))->second;
  }
};

struct Triple {
  Triple() : n1(0), n2(0), n3(0) {}
  Triple(unsigned int value1, unsigned int value2, unsigned int value3) : n1(value1), n2(value2), n3(value3) {}
  Triple(const Triple& other) = default;

  bool operator<(const Triple& other) const
  {
    return n1 == other.n1 ? (n2 == other.n2 ? n3 < other.n3 : n2 < other.n2) : n1 < other.n1;
  }

  bool operator==(const Triple& other) const
  {
    return n1 == other.n1 && n2 == other.n2 && n3 == other.n3;
  }

  unsigned int n1;
  unsigned int n2;
  unsigned int n3;
};

} // namespace infomap

#endif /* NETWORK_H_ */
