/*
 * IDataConverter.h
 *
 *  Created on: Mar 7, 2012
 *      Author: daniel
 */

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
