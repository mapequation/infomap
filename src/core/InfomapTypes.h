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
#include "../io/Config.h"
#include <utility>


namespace infomap {

class M1Infomap : public InfomapOptimizer<MapEquation>
{
public:
	M1Infomap() : InfomapOptimizer<MapEquation>() {}
	M1Infomap(const Config& conf) : InfomapOptimizer<MapEquation>(conf) {}
};

class M2Infomap : public InfomapOptimizer<MemMapEquation>
{
public:
	M2Infomap() : InfomapOptimizer<MemMapEquation>() {}
	M2Infomap(const Config& conf) : InfomapOptimizer<MemMapEquation>(conf) {}
};

}

#endif /* MODULES_CLUSTERING_CLUSTERING_INFOMAPTYPES_H_ */
