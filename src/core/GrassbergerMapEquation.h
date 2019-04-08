/*
 * IntegerMapEquation.h
 */

#ifndef _GRASSBERGERMAPEQUATION_H_
#define _GRASSBERGERMAPEQUATION_H_

#include "IntegerMapEquation.h"
#include <vector>

namespace infomap {

class GrassbergerMapEquation : public IntegerMapEquation {
	using Base = IntegerMapEquation;
public:
	using FlowDataType = FlowDataInt;
	using DeltaFlowDataType = DeltaFlowInt;
	using NodeType = Node<FlowDataType>;

	GrassbergerMapEquation() : IntegerMapEquation() {}

	GrassbergerMapEquation(const GrassbergerMapEquation& other)
	:	IntegerMapEquation(other)
	{}

	GrassbergerMapEquation& operator=(const GrassbergerMapEquation& other) {
		Base::operator =(other);
		return *this;
	}

	virtual ~GrassbergerMapEquation() {}

	virtual void initNetwork(NodeBase& root);

protected:

	double log2(unsigned int d) const;
	virtual double plogp(unsigned int d) const;
	virtual double plogpN(unsigned int d, unsigned int N) const;

	std::vector<double> m_grassbergerLog2;

};



}

#endif /* _GRASSBERGERMAPEQUATION_H_ */
