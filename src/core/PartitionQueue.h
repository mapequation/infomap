/*
 * PartitionQueue.h
 *
 *  Created on: 9 apr 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_CLUSTERING_PARTITIONQUEUE_H_
#define SRC_CLUSTERING_CLUSTERING_PARTITIONQUEUE_H_

#include "InfoNode.h"

namespace infomap {

struct PendingModule
{
	PendingModule() : module(nullptr) {}
	PendingModule(InfoNode* m) : module(m) {}
	InfoNode& operator*() { return *module; }
	InfoNode* module;
};

#include <deque>
class PartitionQueue
{
public:
	typedef std::deque<PendingModule>::size_type size_t;
	PartitionQueue() :
		level(1),
		numNonTrivialModules(0),
		flow(0.0),
		nonTrivialFlow(0.0),
		skip(false),
		indexCodelength(0.0),
		leafCodelength(0.0),
		moduleCodelength(0.0)
	{}

	void swap(PartitionQueue& other)
	{
		std::swap(level, other.level);
		std::swap(numNonTrivialModules, other.numNonTrivialModules);
		std::swap(flow, other.flow);
		std::swap(nonTrivialFlow, other.nonTrivialFlow);
		std::swap(skip, other.skip);
		std::swap(indexCodelength, other.indexCodelength);
		std::swap(leafCodelength, other.leafCodelength);
		std::swap(moduleCodelength, other.moduleCodelength);
		m_queue.swap(other.m_queue);
	}

	size_t size() { return m_queue.size(); }
	void resize(size_t size) { m_queue.resize(size); }
	PendingModule& operator[](size_t i) { return m_queue[i]; }

	unsigned int level;
	unsigned int numNonTrivialModules;
	double flow;
	double nonTrivialFlow;
	bool skip;
	double indexCodelength; // Consolidated
	double leafCodelength; // Consolidated
	double moduleCodelength; // Left to improve on next level

private:
	std::deque<PendingModule> m_queue;
};

}

#endif /* SRC_CLUSTERING_CLUSTERING_PARTITIONQUEUE_H_ */
