/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013 Daniel Edler, Martin Rosvall
 
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
	typedef FlowUndirected		flow_type;
	typedef bool2type<false>	directed_type;
	typedef bool2type<true>		detailedBalance_type;
	static const bool directed = false;
	static const bool detailed_balance = true;
};

class InfomapDirected;

template<>
struct flowData_traits<InfomapDirected>
{
	typedef FlowDirectedWithTeleportation	flow_type;
	typedef bool2type<true> 				directed_type;
	typedef bool2type<true> 				detailedBalance_type;
	static const bool directed = true;
	static const bool detailed_balance = true;
};

class InfomapUndirdir;

template<>
struct flowData_traits<InfomapUndirdir>
{
	typedef FlowDirectedNonDetailedBalance	flow_type;
	typedef bool2type<true> 				directed_type;
	typedef bool2type<false> 				detailedBalance_type;
	static const bool directed = true;
	static const bool detailed_balance = false;
};

class InfomapDirectedUnrecordedTeleportation;

template<>
struct flowData_traits<InfomapDirectedUnrecordedTeleportation>
{
	typedef FlowDirectedNonDetailedBalanceWithTeleportation	flow_type;
	typedef bool2type<true> 				directed_type;
	typedef bool2type<false> 				detailedBalance_type;
	static const bool directed = true;
	static const bool detailed_balance = false;
};


#endif /* FLOWDATA_TRAITS_H_ */
