/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef INFOMATH_H_
#define INFOMATH_H_

#include <cmath>

namespace infomath
{
	inline
	double log2(double p)
	{
		return std::log(p) * M_LOG2E; // M_LOG2E == 1 / M_LN2
	}

	inline
	double plogp(double p)
	{
		return p > 0.0 ? p * log2(p) : 0.0;
	}


	/**
	 * Get a random permutation of indices of the size of the input vector
	 */
	inline
	void getRandomizedIndexVector(std::vector<unsigned int>& randomOrder, MTRand& randGen)
	{
		unsigned int size = randomOrder.size();
		for(unsigned int i = 0; i < size; ++i)
			randomOrder[i] = i;
		for(unsigned int i = 0; i < size; ++i)
			std::swap(randomOrder[i], randomOrder[i + randGen.randInt(size - i - 1)]);
	}
}

#endif /* INFOMAPTH_H_ */
