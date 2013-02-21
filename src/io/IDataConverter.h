/* -----------------------------------------------------------------------

 Infomap software package for multi-level network clustering

   * Copyright (c) 2013. See LICENSE for more info.
   * For credits and origins, see AUTHORS or www.mapequation.org/about.

----------------------------------------------------------------------- */

#ifndef IDATACONVERTER_H_
#define IDATACONVERTER_H_
#include <string>
#include "Config.h"
#include "IData.h"

class IDataConverter
{
public:
	virtual ~IDataConverter() {};

	virtual void readData(std::string filename, IData& data, Config const& config) = 0;
};

#endif /* IDATACONVERTER_H_ */
