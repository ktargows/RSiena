/******************************************************************************
 * SIENA: Simulation Investigation for Empirical Network Analysis
 *
 * Web: http://www.stats.ox.ac.uk/~snijders/siena/
 *
 * File: CovariateIndirectTiesEffect.cpp
 *
 * Description: This file contains the implementation of the class
 * CovariateIndirectTiesEffect.
 *****************************************************************************/
#include <R.h>
#include <stdexcept>

#include "CovariateIndirectTiesEffect.h"
#include "data/Network.h"
#include "data/NetworkLongitudinalData.h"
#include "data/IncidentTieIterator.h"
#include "model/variables/NetworkVariable.h"
#include "model/tables/ConfigurationTable.h"

namespace siena
{

/**
 * Constructor.
 */
CovariateIndirectTiesEffect::CovariateIndirectTiesEffect(
	const EffectInfo * pEffectInfo) :
		CovariateDependentNetworkEffect(pEffectInfo)
{
}


/**
 * Calculates the contribution of a tie flip to the given actor.
 */
double CovariateIndirectTiesEffect::calculateTieFlipContribution(int alter)
	const
{
	double change = 0;

	// If there are enough two-paths from the ego i to the alter j, then
	// we loose the distance 2 pair (i,j) by introducing the tie between
	// them.

	if (this->pVariable()->pTwoPathTable()->get(alter) != 0)
	{
		change -= this->value(alter);
	}

	// This variable is to simplify the later tests if a two-path through
	// the given alter makes a difference.

	int criticalTwoPathCount = 0;

	if (this->pVariable()->outTieExists(alter))
	{
		criticalTwoPathCount = 1;
	}

	// Consider each outgoing tie of the alter j.

	for (IncidentTieIterator iter =
			this->pVariable()->pNetwork()->outTies(alter);
		iter.valid();
		iter.next())
	{
		int h = iter.actor();

		// If h is not the ego i, there's no tie from i to h, and the
		// introduction or withdrawal of the tie (i,j) makes a difference
		// for the pair <i,h> to be a valid distance two pair,
		// then increment the contribution.

		if (h != this->pVariable()->ego() &&
			!this->pVariable()->outTieExists(h) &&
			this->pVariable()->pTwoPathTable()->get(h) == criticalTwoPathCount)
		{
			change += this->value(h);
		}
	}

	// For dissolutions of ties the contribution works in the opposite way.

	if (this->pVariable()->outTieExists(alter))
	{
		change = -change;
	}
	return change;
}


/**
 * Returns if the given configuration table is used by this effect
 * during the calculation of tie flip contributions.
 */
bool CovariateIndirectTiesEffect::usesTable(const ConfigurationTable * pTable)
	const
{
	return pTable == this->pVariable()->pTwoPathTable();
}


/**
 * Returns the statistic corresponding to this effect as part of
 * the evaluation function with respect to the given network.
 */
double CovariateIndirectTiesEffect::evaluationStatistic(Network * pNetwork)
	const
{
	double statistic = 0;
	int n = pNetwork->n();

	const Network * pStartMissingNetwork =
		this->pData()->pMissingTieNetwork(this->period());
	const Network * pEndMissingNetwork =
		this->pData()->pMissingTieNetwork(this->period() + 1);

	// A helper array of marks

	int * mark = new int[n];

	for (int i = 0; i < n; i++)
	{
		mark[i] = -1;
	}

	// Treat the distance-two pairs <i,h> for each i separately.

	for (int i = 0; i < n; i++)
	{
		// Invariant: mark[h] = i if and only if a two-path from i
		// to h has been found.

		// Traverse all two-paths from i

		for (IncidentTieIterator iterI = pNetwork->outTies(i);
			iterI.valid();
			iterI.next())
		{
			int j = iterI.actor();

			for (IncidentTieIterator iterJ = pNetwork->outTies(j);
				iterJ.valid();
				iterJ.next())
			{
				int h = iterJ.actor();

				if (mark[h] < i)
				{
					// The first two-path from i to h is found.

					mark[h] = i;
					statistic += this->value(h);
				}
			}
		}

		// Okay, if there's a tie (i,h) then <i,h> cannot possibly be a
		// distance-two pair. Hence we iterate over outgoing ties (i,h) of i,
		// and if value(h) has been added to the statistic, we subtract it.

		for (IncidentTieIterator iter = pNetwork->outTies(i);
			iter.valid();
			iter.next())
		{
			int h = iter.actor();

			if (mark[h] == i)
			{
				mark[h] = -1;
				statistic -= this->value(h);
			}
		}

		// We do a similar fix for missing ties (i,h) at either end of
		// the period.

		for (IncidentTieIterator iter = pStartMissingNetwork->outTies(i);
			iter.valid();
			iter.next())
		{
			int h = iter.actor();

			if (mark[h] == i)
			{
				mark[h] = -1;
				statistic -= this->value(h);
			}
		}

		for (IncidentTieIterator iter = pEndMissingNetwork->outTies(i);
			iter.valid();
			iter.next())
		{
			int h = iter.actor();

			if (mark[h] == i)
			{
				mark[h] = -1;
				statistic -= this->value(h);
			}
		}

		// Ignore the trivial pair <i,i>.

		if (mark[i] == i)
		{
			statistic -= this->value(i);
		}
	}

	delete[] mark;

	return statistic;
}


/**
 * Returns the statistic corresponding to this effect as part of
 * the endowment function with respect to an initial network
 * and a network of lost ties. The current network is implicit as
 * the introduced ties are not relevant for calculating
 * endowment statistics.
 */
double CovariateIndirectTiesEffect::endowmentStatistic(Network * pInitialNetwork,
	Network * pLostTieNetwork) const
{
	throw logic_error(
		"CovariateIndirectTiesEffect: Endowment effect not supported");
}

}