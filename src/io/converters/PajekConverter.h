/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef PAJEKCONVERTER_H_
#define PAJEKCONVERTER_H_
#include "../IDataConverter.h"
#include "../Config.h"
#include "../IData.h"

class PajekConverter : public IDataConverter
{
public:
	void readData(std::string filename, IData& data, const Config& config);
};

#endif /* PAJEKCONVERTER_H_ */
