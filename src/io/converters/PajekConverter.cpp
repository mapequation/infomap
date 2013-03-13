/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "PajekConverter.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include "../SafeFile.h"
#include "../convert.h"

using std::string;


/**
 * Read network in Pajek format with nodes ordered 1, 2, 3, ..., N,
 * each undirected link occurring only once, and link weights > 0.
 * (if a link is defined more than once, weights are aggregated)
 * For more information, see http://vlado.fmf.uni-lj.si/pub/networks/pajek/.
 * Example network with three nodes and three undirected weighted links:
 * *Vertices 3
 * 1 "Name of first node"
 * 2 "Name of second node"
 * 3 "Name of third node"
 * *Edges 3
 * 1 2 1.0
 * 1 3 3.3
 * 2 3 2.2
 */
void PajekConverter::readData(string filename, IData& data, const Config& config)
{
	string line;
	string buf;
	SafeInFile input(filename.c_str());
	std::cout << "Reading network '" << filename << "'... " << std::flush;

	if( getline(input,line) == NULL)
		throw FileFormatError("Can't read first line of pajek file.");

	unsigned int numNodes = 0;
	std::istringstream ss;
	ss.str(line);
	ss >> buf;
	if(buf == "*Vertices" || buf == "*vertices" || buf == "*VERTICES") {
		if (!(ss >> numNodes))
			throw BadConversionError("Can't parse an integer after \"" + buf + "\" as the number of nodes.");
	}
	else {
		throw FileFormatError("The first line (to lower cases) doesn't match *vertices.");
	}

	if (numNodes == 0)
		throw FileFormatError("The number of vertices cannot be zero.");

	unsigned int specifiedNumNodes = numNodes;
	if (config.nodeLimit > 0)
		numNodes = config.nodeLimit;

	data.reserveNodeCount(numNodes);

	// Read node names, assuming order 1, 2, 3, ...
	for (unsigned int i = 0; i < numNodes; ++i)
	{
		getline(input, line);
		unsigned int nameStart = line.find_first_of("\"");
		unsigned int nameEnd = line.find_last_of("\"");
		string name;
		double nodeWeight = 1.0;
		if(nameStart < nameEnd) {
			name = string(line.begin() + nameStart + 1, line.begin() + nameEnd);
			line = line.substr(nameEnd + 1);
			ss.clear();
			ss.str(line);
		}
		else {
			ss.clear();
			ss.str(line);
			ss >> buf; // Take away the index from the stream
			ss >> name; // Extract the next token as the name assuming no spaces
		}
		ss >> nodeWeight; // Extract the next token as node weight. If failed, default value is used.
		data.addNewNode(name, nodeWeight);
	}

	if (config.nodeLimit > 0 && numNodes < specifiedNumNodes)
	{
		unsigned int surplus = specifiedNumNodes - numNodes;
		for (unsigned int i = 0; i < surplus; ++i)
			getline(input, line);
	}

	// Read the number of links in the network
	getline(input, line);
	ss.clear();
	ss.str(line);
	ss >> buf;
	if(buf != "*Edges" && buf != "*edges" && buf != "*Arcs" && buf != "*arcs") {
		throw FileFormatError("The first line (to lower cases) after the nodes doesn't match *edges or *arcs.");
	}

	unsigned int numEdges = 0;
	unsigned int numDoubleLinks = 0;
	unsigned int numSelfLinks = 0;
	unsigned int numEdgeLines = 0;
	unsigned int numSkippedEdges = 0;
	//	  map<int,map<int,double> > Links;
//	std::map<pair<int,int>,double> links;
	// Read links in format "from to weight", for example "1 3 2" (all integers) and each undirected link only ones (weight is optional).
	while(getline(input, line) != NULL) {
		++numEdgeLines;
		ss.clear();
		ss.str(line);
		unsigned int linkEnd1;
		if (!(ss >> linkEnd1))
			throw BadConversionError("Can't parse first integer of link number " + numEdgeLines);
		unsigned int linkEnd2;
		if (!(ss >> linkEnd2))
			throw BadConversionError("Can't parse second integer of link number " + numEdgeLines);

		if (linkEnd2 > numNodes || linkEnd1 > numNodes)
		{
			++numSkippedEdges;
			continue;
		}

		if (!config.includeSelfLinks && linkEnd1 == linkEnd2)
		{
			++numSelfLinks;
			continue;
		}

		double linkWeight;
		if (!(ss >> linkWeight))
			linkWeight = 1.0;

		if (!config.zeroBasedNodeNumbers)
		{
			linkEnd1--; // Nodes start at 1, but C++ arrays at 0.
			linkEnd2--;
		}

		// If ignore direction and sort edge points.
		//	    if(linkEnd2 < linkEnd1){
		//	      int tmp = linkEnd1;
		//	      linkEnd1 = linkEnd2;
		//	      linkEnd2 = tmp;
		//	    }

		// Get reference to existing link or create new
		//	    double& weight = links[pair<int,int>(linkEnd1, linkEnd2)];
		//	    weight += linkWeight;

		data.addEdge(linkEnd1, linkEnd2, linkWeight);
		++numEdges;
	}
//	numDoubleLinks = numEdgeLines - numSelfLinks - numEdges;

	std::cout << "done! Found " << specifiedNumNodes << " nodes and " << numEdgeLines << " links.";
	if(numDoubleLinks > 0)
		std::cout << " " << numDoubleLinks << " links was aggregated to existing links.";
	if (numSelfLinks > 0)
		std::cout << " Ignoring " << numSelfLinks << " self links.";
	if (config.nodeLimit > 0)
		std::cout << " Limiting network to " << numNodes << " nodes and " << numEdges <<
		" links.";
	std::cout << std::endl;
}
