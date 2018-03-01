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
#include "infomap/InfomapContext.h"
#include "io/HierarchicalNetwork.h"
#include "infomap/MultiplexNetwork.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

/**
 * Run as stand-alone
 */
int run(const std::string& flags);

/**
 * Run from other C++ code
 */
Config init(const std::string& flags);

int run(Network& input, HierarchicalNetwork& output);


class Infomap {
    public:
    Infomap(const std::string flags)
    : config(init(flags)), network(config), tree(config) {}
	
    void readInputData(std::string filename) {
        try
        {
            network.readInputData(filename);
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

    bool addLink(unsigned int n1, unsigned int n2, double weight = 1.0) {
        return network.addLink(n1, n2, weight);
    }

    /**
	 * Change this network to a bipartite network
	 * @param bipartiteStartIndex Nodes equal to or above this index are treated as feature nodes
	 */
	void setBipartiteNodesFrom(unsigned int bipartiteStartIndex) {
        network.setBipartiteNodesFrom(bipartiteStartIndex);
    }

    int run() {
        try
        {
            InfomapContext context(config);
            context.getInfomap()->run(network, tree);
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            return 1;
        }
        return 0;
    }

    Config config;
    MultiplexNetwork network;
    HierarchicalNetwork tree;
};


class MemInfomap {
    static std::string flagsWithMemory(std::string flags) {
        flags += " --with-memory";
        return flags;
    }

    public:
    MemInfomap(const std::string flags)
    : config(init(flags)), network(config), tree(config) {}

    void readInputData(std::string filename) {
        try
        {
            network.readInputData(filename);
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

    bool addTrigram(unsigned int n1, unsigned int n2, unsigned int n3, double weight = 1.0){
        return network.addStateLink(n1, n2, n2, n3, weight);
    }

    bool addStateLink(unsigned int n1PriorState, unsigned int n1, unsigned int n2PriorState, unsigned int n2, double weight = 1.0) {
        return network.addStateLink(n1PriorState, n1, n2PriorState, n2, weight);
    }

    void addMultiplexLink(int layer1, int node1, int layer2, int node2, double weight = 1.0) {
        network.addMultiplexLink(layer1, node1, layer2, node2, weight);
    }

    int run() {
        try
        {
            InfomapContext context(config);
            context.getInfomap()->run(network, tree);
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            return 1;
        }
        return 0;
    }

    Config config;
    MultiplexNetwork network;
    HierarchicalNetwork tree;
};



#ifdef NS_INFOMAP
}
#endif

#endif /* SRC_INFOMAP_H_ */
