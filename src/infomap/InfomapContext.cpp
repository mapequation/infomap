/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

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
	else if (m_config.undirdir || m_config.rawdir)
		m_infomap = std::auto_ptr<InfomapBase>(new InfomapUndirdir(m_config));
	else if (m_config.unrecordedTeleportation)
		m_infomap = std::auto_ptr<InfomapBase>(new InfomapDirectedUnrecordedTeleportation(m_config));
	else
		m_infomap = std::auto_ptr<InfomapBase>(new InfomapDirected(m_config));
}
