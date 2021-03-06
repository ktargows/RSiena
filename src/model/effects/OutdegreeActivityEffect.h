/******************************************************************************
 * SIENA: Simulation Investigation for Empirical Network Analysis
 *
 * Web: http://www.stats.ox.ac.uk/~snijders/siena/
 *
 * File: OutdegreeActivityEffect.h
 *
 * Description: This file contains the definition of the
 * OutdegreeActivityEffect class.
 *****************************************************************************/

#ifndef OUTDEGREEACTIVITYEFFECT_H_
#define OUTDEGREEACTIVITYEFFECT_H_

#include "NetworkEffect.h"

namespace siena
{

/**
 * This class defines the outdegree activity effect defined by
 * s_i(x)= x_{i+}^2. The corresponding statistic is
 * the sum of squared outdegrees over all actors.
 */
class OutdegreeActivityEffect : public NetworkEffect
{
public:
	OutdegreeActivityEffect(const EffectInfo * pEffectInfo);

	virtual double calculateContribution(int alter) const;

protected:
	virtual double tieStatistic(int alter);
};

}

#endif /*OUTDEGREEACTIVITYEFFECT_H_*/
