/*
 * PajekConverter.h
 *
 *  Created on: Aug 2, 2011
 *      Author: daniel
 */

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
