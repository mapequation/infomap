/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall
 
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

#ifndef REUSABLE_VECTOR_H_
#define REUSABLE_VECTOR_H_

#include <vector>
#include <limits>

namespace infomap {

template<typename T>
class ReusableVector
{
public:
    
	ReusableVector(unsigned int size = 0) :
        values(size),
        redirect(size, 0),
        maxOffset(std::numeric_limits<unsigned int>::max() - 1 - size)
    {}

    void startRound() {
        if (offset > maxOffset)
		{
			redirect.assign(numNodes, 0);
			offset = 1;
		}
    }

    void add(T value) {

    }

private:
    std::vector<T> values;
	std::vector<unsigned int> redirect;
	unsigned int maxOffset = std::numeric_limits<unsigned int>::max() - 1;
	unsigned int offset = 1;
};

}

#endif /* REUSABLE_VECTOR_H_ */
