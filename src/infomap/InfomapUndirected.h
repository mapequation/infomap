/* -----------------------------------------------------------------------

 Infomap software package for multi-level network clustering

   * Copyright (c) 2013. See LICENSE for more info.
   * For credits and origins, see AUTHORS or www.mapequation.org/about.

----------------------------------------------------------------------- */

#ifndef INFOMAPUNDIRECTED_H_
#define INFOMAPUNDIRECTED_H_
#include "InfomapGreedy.h"
#include "flowData.h"

class InfomapUndirected : public InfomapGreedy<InfomapUndirected>
{
	friend class InfomapGreedy<InfomapUndirected>;
	typedef Node<FlowType>							NodeType;
public:
	InfomapUndirected(const Config& conf) : InfomapGreedy<InfomapUndirected>(conf) {}
	virtual ~InfomapUndirected() {}

protected:
	virtual void calculateFlow();
	virtual unsigned int optimizeModulesImpl();
	virtual unsigned int moveNodesToPredefinedModulesImpl();
//	virtual void generateNetworkFromChildren(NodeBase* parent);
	virtual void writeLeafNetwork(std::ostream& out);

private:
	void updateCodelength(NodeType& current, double deltaExitOwnModule,
			unsigned int newModule, double deltaExitNewModule);

};

#endif /* INFOMAPUNDIRECTED_H_ */
