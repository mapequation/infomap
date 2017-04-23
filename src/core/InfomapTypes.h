/*
 * InfomapTypes.h
 *
 *  Created on: 9 mar 2015
 *      Author: Daniel
 */

#ifndef MODULES_CLUSTERING_CLUSTERING_INFOMAPTYPES_H_
#define MODULES_CLUSTERING_CLUSTERING_INFOMAPTYPES_H_

#include "Infomap.h"
#include "GreedyOptimizer.h"
#include "MapEquation.h"
#include "MemMapEquation.h"

namespace infomap {

template<typename Node>
class M1Infomap : public Infomap<Node, GreedyOptimizer<Node, MapEquation<Node>>>
{
public:
	M1Infomap() : Infomap<Node, GreedyOptimizer<Node, MapEquation<Node>>>() {}
};

template<typename Node>
class M2Infomap : public Infomap<Node, GreedyOptimizer<Node, MemMapEquation<Node>>>
{
public:
	M2Infomap() : Infomap<Node, GreedyOptimizer<Node, MemMapEquation<Node>>>() {}
};

}

#endif /* MODULES_CLUSTERING_CLUSTERING_INFOMAPTYPES_H_ */
