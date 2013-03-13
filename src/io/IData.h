/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef IDATA_H_
#define IDATA_H_
#include <string>

class IData
{
public:
	virtual ~IData() {}

	virtual void reserveNodeCount(unsigned int nodeCount) = 0;
	virtual void reserveEdgeCount(unsigned int edgeCount) = 0;
	virtual void addNewNode(std::string name, double nodeWeight) = 0;
	virtual void addEdge(unsigned int sourceIndex, unsigned int targetIndex, double weight, double flow = 0.0) = 0;
};

#endif /* IDATA_H_ */
