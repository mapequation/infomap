/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "DataConverterHelper.h"
#include "converters/PajekConverter.h"
#include "convert.h"
#include "../utils/FileURI.h"
#include "converters/LinkListConverter.h"

void DataConverterHelper::readData(IData& data, const Config& config)
{
	FileURI inFilename(config.networkFile, false);
	std::string format = config.inputFormat;

	if (format == "")
	{
		std::string type = inFilename.getExtension();
		if (type == "net")
			format = "pajek";
		else if (type == "txt")
			format = "link-list";
	}
	if (format == "")
		throw UnknownFileTypeError("No known input format specified or implied by file extension.");
	std::auto_ptr<IDataConverter> converter(getConverter(format));
	converter->readData(config.networkFile, data, config);
}

std::auto_ptr<IDataConverter> DataConverterHelper::getConverter(std::string format)
{
	if (format == "pajek")
	{
		return std::auto_ptr<IDataConverter>(new PajekConverter());
	}
	else if (format == "link-list")
	{
		return std::auto_ptr<IDataConverter>(new LinkListConverter());
	}
	else
	{
		throw UnknownFileTypeError("No known input format specified or implied by file extension.");
	}
}
