/*
 * TreeDataWriter.h
 *
 *  Created on: Aug 19, 2011
 *      Author: daniel
 */

#ifndef TREEDATAWRITER_H_
#define TREEDATAWRITER_H_
#include <iostream>
#include "../infomap/TreeData.h"

class TreeDataWriter
{
public:
	typedef unsigned int uint;

	TreeDataWriter(const TreeData&);

	void writeTree(std::ostream& out, bool collapseLeafs = false);
	void writeTopGraph(std::ostream&);
	void writeLeafNodes(std::ostream&);

private:
	const TreeData& m_tree;

};

#endif /* TREEDATAWRITER_H_ */
