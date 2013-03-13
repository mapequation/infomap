/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#ifndef INFOMAPUNDIRDIR_H_
#define INFOMAPUNDIRDIR_H_

#include "InfomapGreedy.h"
#include "flowData.h"

class InfomapUndirdir : public InfomapGreedy<InfomapUndirdir>
{
	friend class InfomapGreedy<InfomapUndirdir>;
	typedef Node<FlowType>												NodeType;
public:
	InfomapUndirdir(const Config& conf);
	virtual ~InfomapUndirdir() {};

protected:
	virtual void calculateFlow();
	virtual unsigned int optimizeModulesImpl();
	virtual unsigned int moveNodesToPredefinedModulesImpl();

	virtual void calculateCodelengthFromActiveNetwork();
	virtual void recalculateCodelengthFromActiveNetwork();

private:
	double getDeltaCodelength(NodeType& current, double additionalExitOldModuleIfMoved,
			unsigned int newModule, double reductionInExitFlowNewModule);
	void updateCodelength(NodeType& current, double deltaInOutOldModule,
			unsigned int newModule, double deltaInOutNewModule);

	//XXX Take back from base after debug
//	double enter_log_enter;
//	double enterFlow;
//	double enterFlow_log_enterFlow;
};

#endif /* INFOMAPUNDIRDIR_H_ */
