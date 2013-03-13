/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "LinkListConverter.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include "../SafeFile.h"
#include "../convert.h"
#include "../../utils/Logger.h"
#include <utility>
#include <algorithm>
#include <limits>

using std::string;
using std::make_pair;


/**
 * Read a list of links where the nodes are defined implicitly from the link indices.
 * Example file:
# Directed graph (each unordered pair of nodes is saved once): web-Stanford.txt
# Stanford web graph from 2002
# Nodes: 281903 Edges: 2312497
# FromNodeId	ToNodeId
1	6548
1	15409
6548	57031
15409	13102
2	17794
 */
void LinkListConverter::readData(string filename, IData& data, const Config& config)
{
	string line;
	string buf;
	std::istringstream ss;

	/* Read network in the format "FromNodeId    ToNodeId    [LinkWeight] ",    */
	/* not assuming a complete list of nodes 1..maxnode                         */

	SafeInFile input(filename.c_str());
	std::cout << "Reading network '" << filename << "'... " << std::flush;
	std::ostringstream e;

	unsigned int NdoubleLinks = 0;
	unsigned int numEdges = 0;
	unsigned int numSkippedEdges = 0;
	bool checkNodeLimit = config.nodeLimit > 0;
	unsigned int maxLinkIndex = 0;

	// Read links in format "from to weight", for example "1 3 0.7"
	unsigned int lineNumber = 0;
	while(getline(input,line) != NULL)
	{
		if (line.empty())
			break;
		++lineNumber;
		if(line[0] == '#')
			continue;
		ss.clear();
		ss.str(line);

		unsigned int linkEnd1;
		if (!(ss >> linkEnd1))
			throw FileFormatError((e << "Can't parse edge source on line " << lineNumber, e.str()));
		unsigned int linkEnd2;
		if (!(ss >> linkEnd2))
			throw FileFormatError((e << "Can't parse edge target on line " << lineNumber << ".", e.str()));

		if (checkNodeLimit && (linkEnd1 > config.nodeLimit || linkEnd2 > config.nodeLimit))
		{
			++numSkippedEdges;
			continue;
		}

		double linkWeight = 1.0;
		ss >> linkWeight; // If nothing to extract, the linkWeight keeps its old value.

		// Assume node numbering starts from 1 if not 0
		if (!config.zeroBasedNodeNumbers)
		{
			ASSERT(linkEnd1 != 0);
			ASSERT(linkEnd2 != 0);
			--linkEnd1;
			--linkEnd2;
		}

		maxLinkIndex = std::max(maxLinkIndex, std::max(linkEnd1, linkEnd2));


		// Aggregate link weights if they are definied more than once
		EdgeMap::iterator fromLink_it = m_links.find(linkEnd1);
		if(fromLink_it == m_links.end()){ // new link
			TargetMap toLink;
			toLink.insert(make_pair(linkEnd2,linkWeight));
			m_links.insert(make_pair(linkEnd1,toLink));
			++numEdges;
		}
		else{
			TargetMap::iterator toLink_it = fromLink_it->second.find(linkEnd2);
			if(toLink_it == fromLink_it->second.end()){ // new link
				fromLink_it->second.insert(make_pair(linkEnd2,linkWeight));
				++numEdges;
			}
			else{
				toLink_it->second += linkWeight;
				++NdoubleLinks;
			}
		}
	}

	if (numEdges == 0)
		throw FileFormatError((e << "Can't find any links", e.str()));

	if (maxLinkIndex == std::numeric_limits<unsigned int>::max())
		throw FileFormatError((e << "Integer overflow, check if you parsed a file " <<
			"with zero-based numbering without proper input options."	, e.str()));

//	unsigned int numNodes = m_links.size(); // Wrong! Will miss dangling nodes.
	unsigned int numNodes = maxLinkIndex + 1;

	//	network.nodeNames = vector<string>(network.Nnode);
	//	network.nodeWeights = vector<double>(network.Nnode,1.0);
	//	network.totNodeWeights = 1.0*network.Nnode;

	data.reserveNodeCount(numNodes);

	for (unsigned int i = 1; i <= numNodes; ++i)
		data.addNewNode(io::stringify(i), 1.0/numNodes);

//	unsigned int nodeCounter = 0;
//	std::map<unsigned int,unsigned int> renumber;
//	bool renum = false;
//	for(EdgeMap::iterator it = m_links.begin();
//			it != m_links.end(); ++it)
//	{
//		//		network.nodeNames[nodeCounter] = to_string(it->first);
//		data.addNewNode(io::stringify(it->first), 1.0/numNodes);
//		renumber.insert(make_pair(it->first,nodeCounter));
//		//		if(nodeCounter != it->first)
//		//			renum = true;
//		++nodeCounter;
//	}
//
//	renum = true;
//	if(renum)
//	{
//		EdgeMap newLinks;
//		EdgeMap::iterator newLinks_it = newLinks.begin();
//		for(EdgeMap::iterator it = m_links.begin();
//				it != m_links.end(); ++it)
//		{
//			unsigned int fromLink = renumber.find(it->first)->second;
//			TargetMap toLink;
//			TargetMap::iterator toLink_it = toLink.begin();
//			for(TargetMap::iterator it2 = it->second.begin();
//					it2 != it->second.end(); ++it2)
//			{
//				toLink_it = toLink.insert(toLink_it,make_pair(renumber.find(it2->first)->second,it2->second));
//			}
//			newLinks_it = newLinks.insert(newLinks_it,make_pair(fromLink,toLink));
//		}
//
//		m_links.swap(newLinks);
//
//	}

	std::cout << "done! (found " << numNodes << " nodes and " << numEdges << " links";
	if(NdoubleLinks > 0)
		std::cout << ", aggregated " << NdoubleLinks << " link(s) defined more than once";

	int NselfLinks = 0;
	for(EdgeMap::iterator fromLink_it = m_links.begin(); fromLink_it != m_links.end(); ++fromLink_it)
	{
		for(TargetMap::iterator toLink_it = fromLink_it->second.begin();
				toLink_it != fromLink_it->second.end(); ++toLink_it)
		{
			unsigned int from = fromLink_it->first;
			unsigned int to = toLink_it->first;
			double weight = toLink_it->second;
			if(weight > 0.0)
			{
				if(from == to)
				{
					NselfLinks++;
				}
				else
				{
					data.addEdge(from, to, weight);
				}
			}
		}
	}

	std::cout << ", ignoring " <<  NselfLinks << " self link(s).";
	if (numSkippedEdges > 0)
		std::cout << " Skipped " << numSkippedEdges << " edge lines due to specified node limit.";
	std::cout << ")" << std::endl;
}

//// Read size from the commented header...
//void LinkListConverter::readData(string filename, IData& data, const Config& config)
//{
//	string line;
//	string buf;
//	SafeInFile input(filename.c_str());
//	std::cout << "Reading network '" << filename << "'... " << std::flush;
//	std::ostringstream e;
//
//	bool foundSize = false;
//	unsigned int numNodes = 0;
//	unsigned int numEdges = 0;
//	unsigned int lineNumber = 0;
//	while(getline(input,line))
//	{
//		++lineNumber;
//		if (line.at(0) != '#')
//			break;
//		size_t pos = line.find("Nodes:");
//		if (pos != string::npos)
//		{
//			string strNodes = line.substr(pos + 6);
//			try {
//				numNodes = io::stringToValue<unsigned int>(strNodes);
//			} catch (const BadConversion& error) {
//				throw FileFormatError((e << "Can't read number of nodes from line " << lineNumber << ".", e.str()));
//			}
//			pos = line.find("Edges:");
//			if (pos == string::npos) {
//				throw FileFormatError((e << "Can't read number of edges from line " << lineNumber << ".", e.str()));
//			}
//			string strEdges = line.substr(pos + 6);
//			try {
//				numEdges = io::stringToValue<unsigned int>(strEdges);
//			} catch (const BadConversion& error) {
//				throw FileFormatError((e << "Can't read number of edges from line " << lineNumber << ".", e.str()));
//			}
//			foundSize = true;
//		}
//	}
//
//	if (!foundSize)
//		throw FileFormatError("Can't read the number of nodes and edges from the file. " +
//				string("Expects a line formatted as '# Nodes: x Edges: y', where x and y are positive integers."));
//
//	std::cout << "Size: " << numNodes << " nodes and " << numEdges << " edges." << std::endl;
//
//	// Create nodes
//	data.reserveNodeCount(numNodes);
//	for (unsigned int i = 0; i < numNodes; ++i)
//	{
//		data.addNewNode("", 1.0);
//	}
//
//	unsigned int numActualEdges = 0;
//	do
//	{
////		std::cout << "Line " << lineNumber << ": '" << line << "'";
//		std::istringstream iss(line);
//		unsigned int source;
//		if (!(iss >> source))
//			throw FileFormatError((e << "Can't parse edge source on line " << lineNumber, e.str()));
////		std::cout << ", source: " << source;
//		unsigned int target;
//		if (!(iss >> target))
//			throw FileFormatError((e << "Can't parse edge target on line " << lineNumber << ".", e.str()));
////		std::cout << ", target: " << target;
////		double weight;
////		if (!(iss >> weight))
////			throw FileFormatError((e << "Can't parse edge target on line " << lineNumber << ".", e.str()));
////		std::cout << ", weight: " << weight << std::endl;
//		data.addEdge(source-1, target-1, 1.0);
//		++lineNumber;
//		++numActualEdges;
//	} while (getline(input, line));
//
//
//	std::cout << "done! Created network with " << numNodes << " nodes and " << numActualEdges << " links." << std::endl;
//}
