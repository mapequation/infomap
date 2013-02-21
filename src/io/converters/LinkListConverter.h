/* -----------------------------------------------------------------------

 Infomap software package for multi-level network clustering

   * Copyright (c) 2013. See LICENSE for more info.
   * For credits and origins, see AUTHORS or www.mapequation.org/about.

----------------------------------------------------------------------- */

#ifndef LINKLISTCONVERTER_H_
#define LINKLISTCONVERTER_H_

#include "../IDataConverter.h"
#include "../Config.h"
#include "../IData.h"
#include <map>

class LinkListConverter: public IDataConverter
{
public:
	typedef std::map<unsigned int, double>		TargetMap;
	typedef std::map<unsigned int, TargetMap >	EdgeMap;

	void readData(std::string filename, IData& data, const Config& config);


private:
	EdgeMap m_links;
};

#endif /* LINKLISTCONVERTER_H_ */
