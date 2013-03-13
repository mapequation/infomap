/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef INFOMAPDIRECTEDUNRECORDEDTELEPORTATION_H_
#define INFOMAPDIRECTEDUNRECORDEDTELEPORTATION_H_
#include "InfomapGreedy.h"
#include "flowData.h"

class InfomapDirectedUnrecordedTeleportation: public InfomapGreedy<InfomapDirectedUnrecordedTeleportation>
{
	friend class InfomapGreedy<InfomapDirectedUnrecordedTeleportation>;
	typedef Node<FlowType>							NodeType; // Redefinition needed, see below
public:
	InfomapDirectedUnrecordedTeleportation(const Config& conf);
	virtual ~InfomapDirectedUnrecordedTeleportation() {}

protected:
	virtual void calculateFlow();
	virtual unsigned int optimizeModulesImpl();
	virtual unsigned int moveNodesToPredefinedModulesImpl();

	virtual void calculateCodelengthFromActiveNetwork();
	virtual void recalculateCodelengthFromActiveNetwork();

private:
	double getDeltaCodelength(NodeType& current, double additionalExitOldModuleIfMoved,
			unsigned int newModule, double reductionInExitFlowNewModule);
	// BUG (lookup order issue) in Eclipse Mac GCC, inherited typedef yields Node<FlowUndirected> in cpp-file, but not here.
	// Thus gives 'Member declaration not found', and for the calls suggests above declaration. No error in XCode.
	void updateCodelength(NodeType& current, double additionalExitOldModuleIfMoved,
			unsigned int newModule, double reductionInExitFlowNewModule);

	//XXX Take back from base after debug
//	double enter_log_enter;

//	double enterFlow;
//	double enterFlow_log_enterFlow;
};

#endif /* INFOMAPDIRECTEDUNRECORDEDTELEPORTATION_H_ */
