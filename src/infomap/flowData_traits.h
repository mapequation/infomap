/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef FLOWDATA_TRAITS_H_
#define FLOWDATA_TRAITS_H_
#include "flowData.h"

// A boolean type as function templates can't be partially specialized.
template<bool> struct bool2type {};


template<typename InfomapType>
struct flowData_traits;
//{
//	typedef FlowUndirected	flow_type;
//};

class InfomapUndirected;

template<>
struct flowData_traits<InfomapUndirected>
{
	typedef FlowUndirected	flow_type;
	typedef bool2type<false> directed_type;
	static const bool directed = false;
};

class InfomapDirected;

template<>
struct flowData_traits<InfomapDirected>
{
	typedef FlowDirectedWithTeleportation	flow_type;
	typedef bool2type<true> 				directed_type;
	static const bool directed = true;
};

class InfomapUndirdir;

template<>
struct flowData_traits<InfomapUndirdir>
{
	typedef FlowDirectedNonDetailedBalance	flow_type;
	typedef bool2type<true> 				directed_type;
	static const bool directed = true;
};

class InfomapDirectedUnrecordedTeleportation;

template<>
struct flowData_traits<InfomapDirectedUnrecordedTeleportation>
{
	typedef FlowDirectedNonDetailedBalanceWithTeleportation	flow_type;
	typedef bool2type<true> 				directed_type;
	static const bool directed = true;
};


#endif /* FLOWDATA_TRAITS_H_ */
