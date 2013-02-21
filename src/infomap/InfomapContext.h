/*
 * InfomapContext.h
 *
 *  Created on: Mar 13, 2012
 *      Author: daniel
 */

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
