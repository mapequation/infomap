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


#include "InfomapContext.h"
#include "InfomapUndirected.h"
#include "InfomapDirected.h"
#include "InfomapUndirdir.h"
#include "InfomapDirectedUnrecordedTeleportation.h"
#include <memory>

InfomapContext::InfomapContext(const Config& config)
:	m_config(config)
{
	if (m_config.isUndirected())
		m_infomap = std::auto_ptr<InfomapBase>(new InfomapUndirected(m_config));
	else if (m_config.undirdir || m_config.outdirdir || m_config.rawdir)
		m_infomap = std::auto_ptr<InfomapBase>(new InfomapUndirdir(m_config));
	else if (m_config.recordedTeleportation)
		m_infomap = std::auto_ptr<InfomapBase>(new InfomapDirected(m_config));
	else
		m_infomap = std::auto_ptr<InfomapBase>(new InfomapDirectedUnrecordedTeleportation(m_config));
}
