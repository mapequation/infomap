/*
 * IntegerMapEquation.h
 */

#ifndef _BAYESMAPEQUATION_H_
#define _BAYESMAPEQUATION_H_

#include "IntegerMapEquation.h"
#include <vector>

namespace infomap {

class BayesMapEquation : public IntegerMapEquation {
	using Base = IntegerMapEquation;
public:
	using FlowDataType = FlowDataInt;
	using DeltaFlowDataType = DeltaFlowInt;
	using NodeType = Node<FlowDataType>;

	BayesMapEquation() : IntegerMapEquation() {}

	BayesMapEquation(const BayesMapEquation& other)
	:	IntegerMapEquation(other)
	{}

	BayesMapEquation& operator=(const BayesMapEquation& other) {
		Base::operator =(other);
		return *this;
	}

	virtual ~BayesMapEquation() {}

	virtual void initNetwork(NodeBase& root);

protected:

	double log2(double d) const;
	virtual double plogp(double d) const;
	virtual double plogpN(double d, double N) const;

};



}

#endif /* _BAYESMAPEQUATION_H_ */
