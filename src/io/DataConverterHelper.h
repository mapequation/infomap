/* -----------------------------------------------------------------------

 Infomap software package for multi-level network clustering

   * Copyright (c) 2013. See LICENSE for more info.
   * For credits and origins, see AUTHORS or www.mapequation.org/about.

----------------------------------------------------------------------- */

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
