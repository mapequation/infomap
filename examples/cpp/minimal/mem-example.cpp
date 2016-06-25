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

#include <iostream>
#include <sstream>
#include <string>

#include <Infomap.h>

void printClusters(infomap::HierarchicalNetwork & tree) {
    std::cout << "\nClusters:\n#physIndex clusterIndex:\n";
    for (infomap::LeafIterator leafIt(&tree.getRootNode()); !leafIt.isEnd(); ++leafIt) {
        std::cout << leafIt->physIndex << " " << leafIt.moduleIndex() << '\n';
    }
}

int main(int argc, char** argv)
{
	infomap::MemInfomap infomapWrapper("--two-level");

	infomapWrapper.addTrigram(0, 2, 0);
	infomapWrapper.addTrigram(0, 2, 1);
	infomapWrapper.addTrigram(1, 2, 1);
	infomapWrapper.addTrigram(1, 2, 0);
	infomapWrapper.addTrigram(1, 2, 3);
	infomapWrapper.addTrigram(3, 2, 3);
	infomapWrapper.addTrigram(2, 3, 4);
	infomapWrapper.addTrigram(3, 2, 4);
	infomapWrapper.addTrigram(4, 2, 4);
	infomapWrapper.addTrigram(4, 2, 3);
	infomapWrapper.addTrigram(4, 3, 3);

	infomapWrapper.run();
	
	printClusters(infomapWrapper.tree);
}
