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


#ifndef SRC_INFOMAP_H_
#define SRC_INFOMAP_H_

#include <string>
#include "io/Config.h"
#include "core/InfomapTypes.h"

namespace infomap {

/**
* Run as stand-alone
*/
int run(const std::string& flags);


class Infomap : public M1Infomap {
    public:
    Infomap() : M1Infomap()
    {}
    Infomap(const Config& conf) : M1Infomap(conf)
    {}
    Infomap(std::string flags, bool requireFileInput = false) :
        Infomap(Config::fromString(flags, requireFileInput))
    {}
};

class MemInfomap : public M2Infomap {
    public:
    MemInfomap() : M2Infomap()
    {}
    MemInfomap(const Config& conf) : M2Infomap(conf)
    {}
    MemInfomap(std::string flags, bool requireFileInput = false) :
        M2Infomap(Config::fromString(flags, requireFileInput))
    {}
};

// class MemInfomap : public M2Infomap {

// }

}

#endif /* SRC_INFOMAP_H_ */
