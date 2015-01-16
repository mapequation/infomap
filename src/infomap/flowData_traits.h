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


#ifndef FLOWDATA_TRAITS_H_
#define FLOWDATA_TRAITS_H_
#include "flowData.h"
#include "Node.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

// A boolean type as function templates can't be partially specialized.
template<bool> struct bool2type {};

struct WithMemory {};
struct WithoutMemory {};

struct DetailedBalance {};
struct NoDetailedBalance {};

struct WithTeleportation {};
struct WithRecordedTeleportation {};
struct WithUnrecordedTeleportation {};
struct WithoutTeleportation {};

struct Directed {};
struct Undirected {};

struct DirectedWithRecordedTeleportation {};
struct NotDirectedWithRecordedTeleportation {};

struct true_type { operator bool() const { return true; }};
struct false_type { operator bool() const { return false; }};

template<typename FlowType, typename NetworkType>
struct InfomapType
{
	typedef FlowType	flow_type;
	typedef NetworkType	network_type;
};


template<typename InfomapType>
struct flowData_traits;

template<>
struct flowData_traits<FlowUndirected>
{
	typedef DetailedBalance								detailed_balance_type;
	typedef NotDirectedWithRecordedTeleportation		directed_with_recorded_teleportation_type;
	typedef WithoutTeleportation						teleportation_type;
	typedef true_type									is_directed_type;
};

template<>
struct flowData_traits<FlowDirectedNonDetailedBalance>
{
	typedef NoDetailedBalance							detailed_balance_type;
	typedef NotDirectedWithRecordedTeleportation		directed_with_recorded_teleportation_type;
	typedef WithoutTeleportation						teleportation_type;
	typedef false_type									is_directed_type;
};

template<>
struct flowData_traits<FlowDirectedWithTeleportation>
{
	typedef DetailedBalance								detailed_balance_type;
	typedef DirectedWithRecordedTeleportation			directed_with_recorded_teleportation_type;
	typedef WithTeleportation							teleportation_type;
	typedef false_type									is_directed_type;
};

template<>
struct flowData_traits<FlowDirectedNonDetailedBalanceWithTeleportation>
{
	typedef NoDetailedBalance							detailed_balance_type;
	typedef NotDirectedWithRecordedTeleportation		directed_with_recorded_teleportation_type;
	typedef WithTeleportation							teleportation_type;
	typedef false_type									is_directed_type;
};



// A traits class template to be able to extract the flow type from incomplete derived classes in the inherited CRTP class
template <typename derived_t>
struct derived_traits;

template<typename FlowType, typename NetworkType>
class InfomapGreedyTypeSpecialized;

template<typename FlowType, typename NetworkType>
struct derived_traits<InfomapGreedyTypeSpecialized<FlowType, NetworkType> > {
	typedef FlowType flow_type;
	typedef Node<FlowType> node_type;
	typedef DeltaFlow deltaflow_type;
};

template<typename FlowType>
struct derived_traits<InfomapGreedyTypeSpecialized<FlowType, WithMemory> > {
	typedef FlowType flow_type;
	typedef MemNode<FlowType> node_type;
	typedef MemDeltaFlow deltaflow_type;
};


template<typename FlowType>
class InfomapGreedySpecialized;

template<typename FlowType>
struct derived_traits<InfomapGreedySpecialized<FlowType> > {
	typedef FlowType flow_type;
};

// // Declare and define a base_traits specialization for derived types:
// template <>
// struct base_traits<InfomapGreedySpecialized<FlowUndirected> > {
//     typedef FlowUndirected flow_type;
// };

// template <>
// struct base_traits<InfomapGreedySpecialized<FlowDirectedNonDetailedBalance> > {
//     typedef FlowDirectedNonDetailedBalance flow_type;
// };

// template <>
// struct base_traits<InfomapGreedySpecialized<FlowDirectedWithTeleportation> > {
//     typedef FlowDirectedWithTeleportation flow_type;
// };

// template <>
// struct base_traits<InfomapGreedySpecialized<FlowDirectedNonDetailedBalanceWithTeleportation> > {
//     typedef FlowDirectedNonDetailedBalanceWithTeleportation flow_type;
// };

#ifdef NS_INFOMAP
}
#endif

#endif /* FLOWDATA_TRAITS_H_ */
