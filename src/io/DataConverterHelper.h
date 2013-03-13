/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef DATACONVERTERHELPER_H_
#define DATACONVERTERHELPER_H_
#include "../io/IData.h"
#include "../io/Config.h"
#include "IDataConverter.h"
#include <memory>
#include <string>

class DataConverterHelper
{
public:

	void readData(IData& data, const Config& config);

private:
	std::auto_ptr<IDataConverter> getConverter(std::string format);

};

#endif /* DATACONVERTERHELPER_H_ */
