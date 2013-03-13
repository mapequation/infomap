/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

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
