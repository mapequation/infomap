/* -----------------------------------------------------------------------

 Infomap software package for multi-level network clustering

   * Copyright (c) 2013. See LICENSE for more info.
   * For credits and origins, see AUTHORS or www.mapequation.org/about.

----------------------------------------------------------------------- */

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
