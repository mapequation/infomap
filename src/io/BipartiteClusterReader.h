/*
 * BipartiteClusterReader.h
 *
 *  Created on: 25 maj 2015
 *      Author: Daniel
 */

#ifndef SRC_IO_BIPARTITECLUSTERREADER_H_
#define SRC_IO_BIPARTITECLUSTERREADER_H_

#include "ClusterReader.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

class BipartiteClusterReader : public ClusterReader {
public:
	BipartiteClusterReader(bool zeroBasedIndexing = false)
	: ClusterReader(zeroBasedIndexing),
	  m_maxFeatureNodeIndex(0)
	{}
	virtual ~BipartiteClusterReader() {}

	const std::map<unsigned int, unsigned int>& featureClusters() const
	{
		return m_featureClusters;
	}

	unsigned int maxFeatureNodeIndex() const
	{
		return m_maxFeatureNodeIndex;
	}

protected:
	virtual void parseClusterLine(std::string line);

	std::map<unsigned int, unsigned int> m_featureClusters;
	unsigned int m_maxFeatureNodeIndex;
};


#ifdef NS_INFOMAP
} /* namespace infomap */
#endif

#endif /* SRC_IO_BIPARTITECLUSTERREADER_H_ */
