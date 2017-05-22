/*
 * InfomapTypes.h
 *
 *  Created on: 9 mar 2015
 *      Author: Daniel
 */

#ifndef MODULES_CLUSTERING_CLUSTERING_INFOMAPTYPES_H_
#define MODULES_CLUSTERING_CLUSTERING_INFOMAPTYPES_H_

#include "InfomapOptimizer.h"
#include "MapEquation.h"
#include "MemMapEquation.h"
#include <utility>


namespace infomap {

class M1Infomap : public InfomapOptimizer<MapEquation>
{
public:
	template<typename... Args>
	M1Infomap(Args&&... args) : InfomapOptimizer<MapEquation>(std::forward<Args>(args)...) {}
};

class M2Infomap : public InfomapOptimizer<MemMapEquation>
{
public:
	template<typename... Args>
	M2Infomap(Args&&... args) : InfomapOptimizer<MemMapEquation>(std::forward<Args>(args)...) {}
};

}

#endif /* MODULES_CLUSTERING_CLUSTERING_INFOMAPTYPES_H_ */
