/*
 * InfoNodeTraits.h
 *
 *  Created on: 24 feb 2015
 *      Author: Daniel
 */

#ifndef SRC_CLUSTERING_INFONODETRAITS_H_
#define SRC_CLUSTERING_INFONODETRAITS_H_

#include <type_traits>
#include <utility> // std::declval
#include "InfomapTypes.h"

namespace infomap {

template<bool _withMemory>
struct default_infomap;

template<>
struct default_infomap<false> {
	template<typename Node>
	using type = M1Infomap<Node>;
};

template<>
struct default_infomap<true> {
	template<typename Node>
	using type = M2Infomap<Node>;
};

}

#endif /* SRC_CLUSTERING_INFONODETRAITS_H_ */
