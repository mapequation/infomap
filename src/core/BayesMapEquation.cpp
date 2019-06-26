/*
 * BayesMapEquation.h
 */

#include "BayesMapEquation.h"
#include "../utils/infomath.h"
#include "../utils/Log.h"
#include "Node.h"
#include <cmath>
#include <gsl/gsl_sf_psi.h>

namespace infomap {

	void BayesMapEquation::initNetwork(NodeBase& root)
	{
		Log(3) << "BayesMapEquation::initNetwork()...\n";
		m_totalDegree = 0.0;
		//m_totalNodes = 0;
		for (NodeBase& node : root)
		{
			m_totalDegree += getNode(node).data.flow;
			//m_totalNodes++;
		}
		//m_totalDegree += m_totalNodes * log(m_totalNodes);

		IntegerMapEquation::initNetwork(root);
	}

	double BayesMapEquation::log2(double d) const
	{
		// return infomath::log2(1.0 * d / m_totalDegree);
		//return gsl_sf_psi(d + 1.0);
                return infomath::log2(d);
	}

	double BayesMapEquation::plogp(double d) const
	{
		// double p = d * 1.0 / m_totalDegree;
		// Log() << " (plogp(" << d << "/" << m_totalDegree << ") ";
		// return infomath::plogpN(d, m_totalDegree);
		return d == 0 ? 0 : d * 1.0 / m_totalDegreePrior * ( log2(d) - log2(m_totalDegreePrior) );// * M_LOG2E;
	}
	double BayesMapEquation::plogpN(double d, double N) const
	{
		// return infomath::plogpN(d, m_totalDegree);
		return d == 0 ? 0 : d * 1.0 / N * ( log2(d) - log2(N) );// * M_LOG2E;
	}
}
