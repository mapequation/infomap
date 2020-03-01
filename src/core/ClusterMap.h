/*
 * ClusterMap.h
 *
 *  Created on: 8 apr 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_CLUSTERING_CLUSTERMAP_H_
#define SRC_CLUSTERING_CLUSTERING_CLUSTERMAP_H_

#include <string>
#include <map>
#include <vector>
#include <deque>

namespace infomap {

using Path = std::deque<unsigned int>; // 1-based indexing
struct NodePath
{
	NodePath(unsigned int nodeId, const Path& path)
	: nodeId(nodeId), path(path) {}
	unsigned int nodeId;
	Path path;
};

// using NodePaths = std::vector<NodePath>;

struct NodePaths
{
	NodePaths(unsigned int size = 0) { reserve(size); }
	void reserve(unsigned int size) { nodePaths.reserve(size); }
	unsigned int size() { return nodePaths.size(); }
	void clear() { nodePaths.clear(); }
	void add(unsigned int nodeId, const Path& path) {
		// path should use 1-based indexing
		add(NodePath(nodeId, path));
	}
	void add(NodePath&& path) {
		nodePaths.push_back(path);
	}
	std::vector<NodePath> nodePaths;
};

class ClusterMap
{
public:
	void readClusterData(const std::string& filename, bool includeFlow = false);

	const std::map<unsigned int, unsigned int>& clusterIds() const {
		return m_clusterIds;
	}

	const std::map<unsigned int, double>& getFlow() const {
		return m_flowData;
	}

	const NodePaths& nodePaths() const { return m_nodePaths; }

	const std::string& extension() const { return m_extension; }

	bool isHigherOrder() const { return m_isHigherOrder; }

protected:
	void readTree(const std::string& filename, bool includeFlow);
	void readClu(const std::string& filename, bool includeFlow);

private:
	std::map<unsigned int, unsigned int> m_clusterIds;
	std::map<unsigned int, double> m_flowData;
	NodePaths m_nodePaths;
	std::string m_extension;
	bool m_isHigherOrder = false;
};

}

#endif /* SRC_CLUSTERING_CLUSTERING_CLUSTERMAP_H_ */
