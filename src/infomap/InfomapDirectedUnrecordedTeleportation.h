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


#ifndef INFOMAPDIRECTEDUNRECORDEDTELEPORTATION_H_
#define INFOMAPDIRECTEDUNRECORDEDTELEPORTATION_H_
#include "InfomapGreedy.h"
#include "flowData.h"

class InfomapDirectedUnrecordedTeleportation: public InfomapGreedy<InfomapDirectedUnrecordedTeleportation>
{
	friend class InfomapGreedy<InfomapDirectedUnrecordedTeleportation>;
	typedef Node<FlowType>							NodeType; // Redefinition needed, see below
public:
	InfomapDirectedUnrecordedTeleportation(const Config& conf);
	virtual ~InfomapDirectedUnrecordedTeleportation() {}

};

#endif /* INFOMAPDIRECTEDUNRECORDEDTELEPORTATION_H_ */
