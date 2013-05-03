/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013 Daniel Edler, Martin Rosvall
 
 For more information, see <http://www.mapequation.org>
 

 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/


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
