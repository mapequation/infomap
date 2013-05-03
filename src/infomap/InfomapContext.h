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


#ifndef INFOMAPCONTEXT_H_
#define INFOMAPCONTEXT_H_
#include "../io/Config.h"
#include "InfomapBase.h"
#include <memory>

class InfomapContext
{
public:
	InfomapContext(const Config& config);

	const std::auto_ptr<InfomapBase>& getInfomap() const
	{
		return m_infomap;
	}

private:
	const Config& m_config;
	std::auto_ptr<InfomapBase> m_infomap;
};

#endif /* INFOMAPCONTEXT_H_ */
