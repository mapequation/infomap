#ifndef __ClusterWriter_H_
#define __ClusterWriter_H_

#include <iostream>
#include <string>
#include "../core/NodeBase.h"
#include "Config.h"
#include "SafeFile.h"

namespace infomap {
    
    static void printTree(NodeBase &root, std::ostream &out = std::cout) {
    	auto* infomapRoot = root.getInfomapRoot();
        out << "# Codelength = " << (infomapRoot != nullptr? infomapRoot->codelength : root.codelength) << " bits.\n";
        auto it = root.begin_treePath();
        it++;
        for (; !it.isEnd(); ++it) {
            NodeBase &node = *it;
            if (node.isLeaf()) {
                auto &path = it.path();
                out << io::stringify(path, ":", 1) << " " << node.data.flow << " \"" << node.stateId << "\" " <<
                		node.physicalId << "\n";
            }
        }
    }

    static void printClu(NodeBase &root, std::ostream &out = std::cout) {
        out << "# Codelength = " << root.codelength << " bits.\n";
        out << "# key clusterIndex flow:.\n";
        for (auto it(root.begin_treePath()); !it.isEnd(); ++it) {
            NodeBase &node = *it;
            if (node.isLeaf()) {
            	 out << node.stateId << " " << it.clusterIndex() << " " << node.data.flow << "\n";
            }
        }
    }


    static void print(NodeBase &root, Config &conf, std::string networkName) {
        if (conf.printTree) {
            SafeOutFile outFile(networkName + ".tree");
            printTree(root, outFile);
            std::cout << "   -> Writed tree to file " << networkName + ".tree\n";
        }
        if (conf.printCluster) {
            SafeOutFile outFile(networkName + ".clu");
            printClu(root, outFile);
            std::cout << "   -> Writed clu to file " << networkName + ".clu\n";
        }
    }
}

#endif //__ClusterWriter_H_
