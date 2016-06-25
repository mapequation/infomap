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
#include <Infomap.h>

void printClusters(infomap::HierarchicalNetwork& tree)
{
	std::cout << "\nClusters:\n#originalIndex clusterIndex:\n";

	for (infomap::LeafIterator leafIt(&tree.getRootNode()); !leafIt.isEnd(); ++leafIt)
		std::cout << leafIt->originalLeafIndex << " " << leafIt.moduleIndex() << '\n';
}

int main(int argc, char** argv)
{
	infomap::Infomap infomapWrapper("--two-level -N2");

	infomapWrapper.addLink(0, 1);
	infomapWrapper.addLink(0, 2);
	infomapWrapper.addLink(0, 3);
	infomapWrapper.addLink(1, 0);
	infomapWrapper.addLink(1, 2);
	infomapWrapper.addLink(2, 1);
	infomapWrapper.addLink(2, 0);
	infomapWrapper.addLink(3, 0);
	infomapWrapper.addLink(3, 4);
	infomapWrapper.addLink(3, 5);
	infomapWrapper.addLink(4, 3);
	infomapWrapper.addLink(4, 5);
	infomapWrapper.addLink(5, 4);
	infomapWrapper.addLink(5, 3);

	infomapWrapper.run();
	
	printClusters(infomapWrapper.tree);
}
