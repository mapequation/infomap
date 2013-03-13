/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef INFOMAPDIRECTED_H_
#define INFOMAPDIRECTED_H_
#include "InfomapGreedy.h"
#include "flowData.h"

class InfomapDirected : public InfomapGreedy<InfomapDirected>
{
	friend class InfomapGreedy<InfomapDirected>;
	typedef flowData_traits<InfomapDirected>::flow_type		FlowType;
	typedef Node<FlowType>									NodeType;
public:
	InfomapDirected(const Config& conf);
	virtual ~InfomapDirected() {}

protected:
	virtual void calculateFlow();
	virtual unsigned int optimizeModulesImpl();
	virtual unsigned int moveNodesToPredefinedModulesImpl();

	virtual void recalculateCodelengthFromConsolidatedNetwork();

private:
	void updateCodelength(NodeType& current, double deltaInOutOldModule,
			unsigned int newModule, double deltaInOutNewModule);

	const double alpha;
	const double beta;
};

#endif /* INFOMAPDIRECTED_H_ */
