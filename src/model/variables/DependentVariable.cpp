/******************************************************************************
 * SIENA: Simulation Investigation for Empirical Network Analysis
 *
 * Web: http://www.stats.ox.ac.uk/~snijders/siena/
 *
 * File: DependentVariable.cpp
 *
 * Description: This file contains the implementation of the
 * DependentVariable class.
 *****************************************************************************/
#include <R_ext/Print.h>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "BehaviorVariable.h"
#include "DependentVariable.h"
#include "utils/Utils.h"
#include "utils/Random.h"
#include "data/ActorSet.h"
#include "data/ConstantCovariate.h"
#include "data/ChangingCovariate.h"
#include "network/Network.h"
#include "network/OneModeNetwork.h"
#include "data/LongitudinalData.h"
#include "data/NetworkLongitudinalData.h"
#include "model/Model.h"
#include "model/EffectInfo.h"
#include "model/EpochSimulation.h"
#include "model/ml/MLSimulation.h"
#include "model/SimulationActorSet.h"
#include "model/effects/AllEffects.h"
#include "model/effects/EffectFactory.h"
#include "model/effects/StructuralRateEffect.h"
#include "model/effects/DiffusionRateEffect.h"
#include "model/variables/NetworkVariable.h"
#include "model/variables/EffectValueTable.h"
#include "network/IncidentTieIterator.h"

namespace siena
{

// ----------------------------------------------------------------------------
// Section: Construction area
// ----------------------------------------------------------------------------

/**
 * Creates a dependent variable for the given set of actors.
 * @param pSimulation the model simulation, which becomes the owner of
 * this variable
 */
DependentVariable::DependentVariable(string name,
	const ActorSet * pActorSet,
	EpochSimulation * pSimulation) : NamedObject(name)
{
	this->lpActorSet = pSimulation->pSimulationActorSet(pActorSet);
	this->lpSimulation = pSimulation;
	this->ltotalRate = 0;
	this->lrate = new double[this->n()];
	this->lcovariateRates = new double[this->n()];
	this->lpEvaluationFunction = new Function();
	this->lpEndowmentFunction = new Function();
	this->lpCreationFunction = new Function();
	this->lacceptances.resize(NBRTYPES, 0);
	this->lrejections.resize(NBRTYPES, 0);
	this->laborts.resize(NBRTYPES, 0);

	NetworkLongitudinalData * pNetworkData =
		dynamic_cast<NetworkLongitudinalData *>(
			pSimulation->pData()->pNetworkData(name));
	if (pNetworkData)
	{
	 	this->lnumberSettings = pNetworkData->rSettingNames().size();
	 	this->lsettingRates = new double[this->lnumberSettings];
	 	this->lsettingProbs = new double[this->lnumberSettings];
	 }
	 else
	{
		this->lnumberSettings = 0;
		this->lsettingRates = 0;
	 	this->lsettingProbs = 0;
	}
	this->lstepType = -1;
}


/**
 * Reads the parameters of rate effects from the model and stores them
 * internally. This method must be called right after the creation of
 * all dependent variables.
 */
void DependentVariable::initializeRateFunction()
{
	const Data * pData = this->lpSimulation->pData();
	const Model * pModel = this->lpSimulation->pModel();

	// initialize setting scores
	if (this->networkVariable())
	{
		NetworkLongitudinalData * pNetworkData =
			dynamic_cast<NetworkLongitudinalData *>(
				this->pSimulation()->pData()->pNetworkData(this->name()));
		const std::vector< string > & rSettingNames =
			pNetworkData->rSettingNames();
		for (unsigned i = 0 ; i < rSettingNames.size(); i++)
		{
			this->lsettingRateScores[rSettingNames[i]] = 0;
		}
	}

	const vector<EffectInfo *> & rRateEffects =
		pModel->rRateEffects(this->name());

	for (unsigned i = 0; i < rRateEffects.size(); i++)
	{
		EffectInfo * pEffectInfo = rRateEffects[i];
		double parameter = pEffectInfo->parameter();
		string effectName = pEffectInfo->effectName();
		string interactionName = pEffectInfo->interactionName1();
		string interactionName2 = pEffectInfo->interactionName2();
		string rateType = pEffectInfo->rateType();

		if (rateType == "covariate")
		{
			//	Rprintf("covariate\n");
			// Covariate-dependent rate effect

			//if (parameter != 0)
			//{
				ConstantCovariate * pConstantCovariate =
					pData->pConstantCovariate(interactionName);
				ChangingCovariate * pChangingCovariate =
					pData->pChangingCovariate(interactionName);
				const BehaviorVariable * pBehaviorVariable =
					(const BehaviorVariable *)
					this->lpSimulation->pVariable(interactionName);

				if (pConstantCovariate)
				{
					if (this->lpActorSet->pOriginalActorSet() !=
						pConstantCovariate->pActorSet())
					{
						throw domain_error("Mismatch of actor sets");
					}

					this->lconstantCovariateParameters[pConstantCovariate] =
						parameter;
					this->lconstantCovariateScores[pConstantCovariate] = 0;
					this->lconstantCovariateSumTerm[pConstantCovariate] = 0;
					this->lconstantCovariateModelBSumTerm[pConstantCovariate]
						= 0;
				}
				else if (pChangingCovariate)
				{
					if (this->lpActorSet->pOriginalActorSet() !=
						pChangingCovariate->pActorSet())
					{
						throw domain_error("Mismatch of actor sets");
					}

					this->lchangingCovariateParameters[pChangingCovariate] =
						parameter;
					this->lchangingCovariateScores[pChangingCovariate] = 0;
					this->lchangingCovariateSumTerm[pChangingCovariate] = 0;
					this->lchangingCovariateModelBSumTerm[pChangingCovariate]
						= 0;
				}
				else if (pBehaviorVariable)
				{
					if (this->lpActorSet != pBehaviorVariable->pActorSet())
					{
						throw domain_error("Mismatch of actor sets");
					}

					this->lbehaviorVariableParameters[pBehaviorVariable] =
						parameter;
					this->lbehaviorVariableScores[pBehaviorVariable] = 0;
					this->lbehaviorVariableSumTerm[pBehaviorVariable] = 0;
					this->lbehaviorVariableModelBSumTerm[pBehaviorVariable]
						= 0;
				}
				else
					throw logic_error(
						"No individual covariate named '" +
						interactionName +
						"'.");
				//}
		}
		else if (rateType == "structural")
		{
			// We expect a structural (network-dependent) rate effect here.

			const NetworkVariable * pVariable;

			if (interactionName == "")
			{
				 pVariable = dynamic_cast<const NetworkVariable *>(this);
			}
			else
			{
				 pVariable = dynamic_cast<const NetworkVariable *>(
					this->lpSimulation->pVariable(interactionName));
			}

			if (!pVariable)
			{
				throw logic_error("The structural rate effect " +
					effectName +
					" for dependent variable " +
					this->name() +
					" refers to a non-existing network variable " +
					interactionName);
			}

			if (effectName == "outRate")
			{
				if (this->lpActorSet != pVariable->pSenders())
				{
					throw std::invalid_argument("Mismatch of actor sets");
				}

				this->lstructuralRateEffects.push_back(
					new StructuralRateEffect(pVariable,
						OUT_DEGREE_RATE,
						parameter));
				this->loutDegreeScores[pVariable] = 0;
				this->loutDegreeSumTerm[pVariable] = 0;
				this->loutDegreeModelBSumTerm[pVariable] = 0;
			}
			else if (effectName == "inRate")
			{
				if (this->lpActorSet != pVariable->pReceivers())
				{
					throw std::invalid_argument("Mismatch of actor sets");
				}

				this->lstructuralRateEffects.push_back(
					new StructuralRateEffect(pVariable,
						IN_DEGREE_RATE,
						parameter));
				this->linDegreeScores[pVariable] = 0;
				this->linDegreeSumTerm[pVariable] = 0;
			}
			else if (effectName == "recipRate")
			{
				if (!pVariable->oneModeNetwork())
				{
					throw std::invalid_argument(
						"One-mode network variable expected");
				}

				if (this->lpActorSet != pVariable->pSenders())
				{
					throw std::invalid_argument("Mismatch of actor sets");
				}

				this->lstructuralRateEffects.push_back(
					new StructuralRateEffect(pVariable,
						RECIPROCAL_DEGREE_RATE,
						parameter));
				this->lreciprocalDegreeScores[pVariable] = 0;
				this->lreciprocalDegreeSumTerm[pVariable] = 0;
			}
			else if (effectName == "outRateInv")
			{
				if (this->lpActorSet != pVariable->pSenders())
				{
					throw std::invalid_argument("Mismatch of actor sets");
				}

				this->lstructuralRateEffects.push_back(
					new StructuralRateEffect(pVariable,
						INVERSE_OUT_DEGREE_RATE,
						parameter));
				this->linverseOutDegreeScores[pVariable] = 0;
				this->linverseOutDegreeSumTerm[pVariable] = 0;
				this->linverseOutDegreeModelBSumTerm[pVariable] = 0;
			}
			else
			{
				throw domain_error("Unexpected rate effect " + effectName);
			}
		}
		else if (rateType == "diffusion")
		{
			const NetworkVariable * pVariable;
			const BehaviorVariable * pBehaviorVariable =
				dynamic_cast<const BehaviorVariable *>(this);

			pVariable = dynamic_cast<const NetworkVariable *>(
				this->lpSimulation->pVariable(interactionName));

			if (!pVariable)
			{
				throw logic_error("The diffusion rate effect " +
					effectName +
					" for dependent variable " +
					this->name() +
					" refers to a non-existing network variable " +
					interactionName);
			}
			if (interactionName2 == "")
			{
				if (effectName == "avExposure" ||
					effectName == "totExposure" ||
					effectName == "susceptAvIn" ||
					effectName == "infectIn" ||
					effectName == "infectOut")
				{
					if (this->lpActorSet != pVariable->pSenders())
					{
						throw std::invalid_argument("Mismatch of actor sets");
					}

					this->ldiffusionRateEffects.push_back(
						new DiffusionRateEffect(pVariable,
							pBehaviorVariable,
							effectName,
							parameter));
				}
				else
				{
					throw domain_error("Unexpected rate effect " + effectName);
				}
			}
			else
			{
				// Covariate-dependent diffusion rate effects

		   		const ConstantCovariate * pConstantCovariate =
					this->lpSimulation->pData()->
					pConstantCovariate(interactionName2);
				const ChangingCovariate * pChangingCovariate =
					this->lpSimulation->pData()->
					pChangingCovariate(interactionName2);

				if (effectName == "susceptAvCovar" ||
					effectName == "infectCovar")
				{
					if (this->lpActorSet != pVariable->pSenders())
					{
						throw std::invalid_argument("Mismatch of actor sets");
					}

					this->ldiffusionRateEffects.push_back(
						new DiffusionRateEffect(pVariable,
							pBehaviorVariable,
							pConstantCovariate,
							pChangingCovariate,
							effectName,
							parameter));
				}
				else
				{
					throw domain_error("Unexpected rate effect " + effectName);
				}
			}
		}

	}
	// If there are no rate effects depending on changing covariates,
    // or behavior variables then the covariate based rates can be calculated
	// just once.

	if (this->lchangingCovariateParameters.empty() &&
		this->lbehaviorVariableParameters.empty())
	{
		this->updateCovariateRates();
	}
}


/**
 * Creates the evaluation effects and stores them in the evaluation function.
 */
void DependentVariable::initializeEvaluationFunction()
{
	this->initializeFunction(this->lpEvaluationFunction,
		this->lpSimulation->pModel()->rEvaluationEffects(this->name()));
}


/**
 * Creates the endowment effects and stores them in the endowment function.
 */
void DependentVariable::initializeEndowmentFunction()
{
	this->initializeFunction(this->lpEndowmentFunction,
		this->lpSimulation->pModel()->rEndowmentEffects(this->name()));
}


/**
 * Creates the tie creation effects and stores them in the creation function.
 */
void DependentVariable::initializeCreationFunction()
{
	this->initializeFunction(this->lpCreationFunction,
		this->lpSimulation->pModel()->rCreationEffects(this->name()));
}


void DependentVariable::initializeFunction(Function * pFunction,
	const vector<EffectInfo *> & rEffects) const
{
	EffectFactory factory(this->pSimulation()->pData());

	for (unsigned i = 0; i < rEffects.size(); i++)
	{
		EffectInfo * pEffectInfo = rEffects[i];
		Effect * pEffect = factory.createEffect(pEffectInfo);
		pFunction->addEffect(pEffect);
	}
}


/**
 * Deallocates this dependent variable.
 */
DependentVariable::~DependentVariable()
{
	delete this->lpEvaluationFunction;
	delete this->lpEndowmentFunction;
	delete this->lpCreationFunction;
	delete[] this->lrate;
	delete[] this->lcovariateRates;
	delete[] this->lsettingRates;
	delete[] this->lsettingProbs;

	// Delete the structural rate effects.
	deallocateVector(this->lstructuralRateEffects);

	// Delete the diffusion rate effects.
	deallocateVector(this->ldiffusionRateEffects);

	// Nullify the fields

	this->lpSimulation = 0;
	this->lrate = 0;
	this->lcovariateRates = 0;
	this->lpEvaluationFunction = 0;
	this->lpEndowmentFunction = 0;
	this->lpCreationFunction = 0;
}


// ----------------------------------------------------------------------------
// Section: Accessors
// ----------------------------------------------------------------------------

/**
 * Returns the number of actors for this variable.
 */
int DependentVariable::n() const
{
	return this->lpActorSet->n();
}


/**
 * Returns the ID of this dependent variable, which is the same as the ID
 * of the underlying observed data object.
 */
int DependentVariable::id() const
{
	return this->pData()->id();
}


/**
 * Stores the distance of this variable to the observed data at the
 * beginning of the current period.
 */
void DependentVariable::simulatedDistance(int distance)
{
	this->lsimulatedDistance = distance;
}


/**
 * Returns the distance of this variable to the observed data at the
 * beginning of the current period.
 */
int DependentVariable::simulatedDistance() const
{
	return this->lsimulatedDistance;
}

/**
 * Returns the number of settings for this dependent variable (only networks)
 */
int DependentVariable::numberSettings() const
{
	return this->lnumberSettings;
}

/**
 * Generates a random step type for the settings model. Or -1 if not relevant.
 * Only ever relevant to networks.
 */
void DependentVariable::getStepType()
{
	if (this->lnumberSettings == 0)
	{
		this->lstepType = -1;
	}
	else
	{
		this->lstepType = nextIntWithProbabilities(this->lnumberSettings,
			this->lsettingProbs);
	}
}
/**
 * Returns the step type for the settings model. -1 if not settings model..
 */
int DependentVariable::stepType() const
{
	return this->lstepType;
}
// ----------------------------------------------------------------------------
// Section: Initialization
// ----------------------------------------------------------------------------

/**
 * Initializes this dependent variable as of the beginning of the given period.
 */
void DependentVariable::initialize(int period)
{
	this->lperiod = period;
	this->lsimulatedDistance = 0;
	this->lbasicRateScore = 0;
	this->lbasicRateDerivative = 0;
	this->lbasicRate =
		this->lpSimulation->pModel()->basicRateParameter(this->pData(),
			period);
	if (this->networkVariable())
	{
		NetworkLongitudinalData * pNetworkData =
			dynamic_cast<NetworkLongitudinalData *>(this->pData());
		const std::vector< string > & rSettingNames =
			pNetworkData->rSettingNames();
		if (this->lnumberSettings > 0)
		{
			double sum = 0;
			for (unsigned i = 0;
				 i < rSettingNames.size();
				 i++)
			{
				this->lsettingRates[i] =
					this->pSimulation()->pModel()->
					settingRateParameter(pNetworkData,
						rSettingNames[i], period);
				sum += this->lsettingRates[i];
			}
			for (unsigned i = 0;
				 i < rSettingNames.size();
				 i++)
			{
				this->lsettingProbs[i] = this->lsettingRates[i] / sum;
			}
			this->lbasicRate = 0;
		}
	}
	if (!this->lchangingCovariateParameters.empty() ||
		!this->lbehaviorVariableParameters.empty())
	{
		// The changing covariates may have different values in different
		// periods, hence the covariate-based rates have to be recalculated.

		// TODO: Behavior variables change all the time, so I'm not sure
		// it is enough to update the covariate rates at the start of the
		// period only. Amended 25/11/2010 RR

		this->updateCovariateRates();
	}

	// Make sure the rates will be calculated whenever calculateRates()
	// is called next time.

	this->lvalidRates = false;
}


// ----------------------------------------------------------------------------
// Section: Rate calculation
// ----------------------------------------------------------------------------

/**
 * Calculates the rate of change or each actor and the total rate.
 */
void DependentVariable::calculateRates()
{
	double sumRatesSquared = 0;
	if (!this->constantRates() || !this->lvalidRates)
	{
		this->ltotalRate = 0;
		int n = this->n();

		for (int i = 0; i < n; i++)
		{
			// If an actor cannot make a change with respect to this variable,
			// then its rate is 0.

			if (this->canMakeChange(i))
			{
				this->lrate[i] = this->calculateRate(i);
			}
			else
			{
				this->lrate[i] = 0;
			}
			this->ltotalRate += this->lrate[i];
			sumRatesSquared += this->lrate[i] * this->lrate[i];

		}

		if (this->pSimulation()->pModel()->needScores())
		{
		    this->calculateScoreSumTerms();
		}
		if(this->symmetric() && this->pSimulation()->pModel()->modelTypeB())
		{
		    this->ltotalRate = this->totalRate() * this->totalRate() -
				sumRatesSquared;
		}

		this->lvalidRates = true;
	}

}


/**
 * Ensures that the rates will be recalculated when the method
 * calculateRates() is called next time.
 */
void DependentVariable::invalidateRates()
{
	this->lvalidRates = false;
}


/**
 * Returns if the given actor can change the current state of this variable.
 */
bool DependentVariable::canMakeChange(int actor) const
{
	return this->lpActorSet->active(actor);
}


/**
 * Returns if the rates are constant over the current period, and can be
 * thus calculated just once.
 */
bool DependentVariable::constantRates() const
{
	return this->lstructuralRateEffects.empty() &&
		this->ldiffusionRateEffects.empty() &&
		this->lbehaviorVariableParameters.empty();
}


/**
 * Calculates the rate of the given actor.
 */
double DependentVariable::calculateRate(int i)
{
	// The rate is the product of the basic rate parameter for the current
	// period, exponentials of some covariate-based effects, and exponentials
	// of some effects depending on the structure of certain networks. The
	// later two components are precomputed for efficiency.

	return (this->basicRate() + this->settingRate()) *
		this->lcovariateRates[i] *
		this->behaviorVariableRate(i) *
		this->structuralRate(i) *
		this->diffusionRate(i);
}


/**
 * Returns the total rate of change over all actors.
 */
double DependentVariable::totalRate() const
{
	return this->ltotalRate;
}


/**
 * Returns the rate of change for the given actor. It is assumed that the
 * rates have been calculated already by calling the method calculateRates.
 */
double DependentVariable::rate(int actor) const
{
	return this->lrate[actor];
}


/**
 * Recalculates the covariate-based components of the rate functions using
 * the current values of parameters and changing covariates.
 */
void DependentVariable::updateCovariateRates()
{
	// Nullify the array.

	for (int i = 0; i < this->n(); i++)
	{
		this->lcovariateRates[i] = 0;
	}

	// Add the contributions of each constant covariate with non-zero parameter
	// for the rate functions. The contribution of a constant covariate v to
	// the rate function of an actor i is exp(alpha v[i]), where alpha is
	// the corresponding parameter. The calculation of the exponentials is
	// postponed, though.

	for (std::map<const ConstantCovariate *, double>::iterator iter =
			this->lconstantCovariateParameters.begin();
		iter != this->lconstantCovariateParameters.end();
		iter++)
	{
		const ConstantCovariate * pCovariate = iter->first;
		double parameter = iter->second;

		for (int i = 0; i < this->n(); i++)
		{
			this->lcovariateRates[i] += parameter * pCovariate->value(i);
		}
	}

	// Add the contributions of each changing covariate with non-zero parameter
	// for the rate functions. The contribution of a changing covariate v to
	// the rate function of an actor i is exp(alpha v[i][h]), where alpha is
	// the corresponding parameter, and h is the current period.
	// Again, the calculation of the exponentials is postponed.

	for (std::map<const ChangingCovariate *, double>::iterator iter =
			this->lchangingCovariateParameters.begin();
		iter != this->lchangingCovariateParameters.end();
		iter++)
	{
		const ChangingCovariate * pCovariate = iter->first;
		double parameter = iter->second;
		for (int i = 0; i < this->n(); i++)
		{
			this->lcovariateRates[i] +=
				parameter * pCovariate->value(i, this->period());
		}
	}

	// Add the contributions of each behavior variable with non-zero parameter
	// for the rate functions. The contribution of a behavior variable v to
	// the rate function of an actor i is exp(alpha v[i]), where alpha is
	// the corresponding parameter.
	// Again, the calculation of the exponentials is postponed.

// 	for (std::map<const BehaviorVariable *, double>::iterator iter =
// 			this->lbehaviorVariableParameters.begin();
// 		iter != this->lbehaviorVariableParameters.end();
// 		iter++)
// 	{
// 		const BehaviorVariable * pBehavior = iter->first;
// 		double parameter = iter->second;

// 		for (int i = 0; i < this->n(); i++)
// 		{
// 			this->lcovariateRates[i] += parameter * pBehavior->value(i);
// 		}
// 	}

	// Okay, now we take the exponentials of the sums of contributions over
	// all covariates. This is valid, because exp(x_1 + ... + x_k) =
	// exp(x_1) * ... * exp(x_k).

	for (int i = 0; i < this->n(); i++)
	{
		this->lcovariateRates[i] = exp(this->lcovariateRates[i]);
	}
}


/**
 * Returns the component of the rate function of actor <i>i</i> depending
 * on structural effects.
 */
double DependentVariable::structuralRate(int i) const
{
	double rate = 1;
	int effectCount = this->lstructuralRateEffects.size();

	for (int effectIndex = 0; effectIndex < effectCount; effectIndex++)
	{
		rate *= this->lstructuralRateEffects[effectIndex]->value(i);
	}

	return rate;
}
/**
 * Returns the component of the rate function of actor <i>i</i> depending
 * on diffusion effects.
 */
double DependentVariable::diffusionRate(int i) const
{
	double rate = 1;
	int effectCount = this->ldiffusionRateEffects.size();

	for (int effectIndex = 0; effectIndex < effectCount; effectIndex++)
	{
		rate *= this->ldiffusionRateEffects[effectIndex]->value(i,this->lperiod);
	}

	return rate;
}
/**
 * Returns the component of the rate function of actor <i>i</i> depending
 * on behavior variables.
 */
double DependentVariable::behaviorVariableRate(int i) const
{
	// Add the contributions of each behavior variable with non-zero parameter
	// for the rate functions. The contribution of a behavior variable v to
	// the rate function of an actor i is exp(alpha v[i]), where alpha is
	// the corresponding parameter.

	double rate = 0;
	for (std::map<const BehaviorVariable *, double>::const_iterator iter =
			this->lbehaviorVariableParameters.begin();
		iter != this->lbehaviorVariableParameters.end();
		iter++)
	{
		const BehaviorVariable * pBehavior = iter->first;
		double parameter = iter->second;
		rate += parameter * pBehavior->value(i);
	}

	return exp(rate);
}


/**
 * Returns the component of the basic rate function for settings
 * (network variables only).
 */
double DependentVariable::settingRate() const
{
	double settingRate = 0;
	for (int i = 1; i < this->lnumberSettings; i++)
	{
		settingRate += this->lsettingRates[i];
	}
	return settingRate;
}

// ----------------------------------------------------------------------------
// Section: Scores
// ----------------------------------------------------------------------------

/**
 * Updates the rate score functions for this event for this variable.
 * @param[in] tau the time increment in the current step of the simulation
 * @param[in] pSelectedVariable the variable, which has been selected in
 * the current step of the simulation (0, if none is selected)
 * @param[in] selectedActor the actor, which has been selected to change
 * the selected variable, if any. If no variable has been selected, this
 * parameter is ignored.
 */
void DependentVariable::accumulateRateScores(double tau,
	const DependentVariable * pSelectedVariable,
	int selectedActor)
{
	// Update the score for the basic rate parameter


	if (this == pSelectedVariable && this->lstepType == -1)
	{
		this->lbasicRateScore += 1.0 / this->basicRate();
		if (this->symmetric() &&
			this->pSimulation()->pModel()->modelTypeB())
		{
			throw logic_error("model type b");
			//this->lbasicRateScore += 1.0 / this->basicRate();
		}
	}
	this->lbasicRateScore -= this->totalRate() * tau / this->basicRate();

	if (this->symmetric() && this->pSimulation()->pModel()->modelTypeB())
	{
		throw logic_error("model type b");
		this->lbasicRateScore -= this->totalRate() * tau / this->basicRate();
	}

	// Update scores for setting rates
	NetworkLongitudinalData * pNetworkData =
		dynamic_cast<NetworkLongitudinalData *>(this->pData());
	const std::vector< string > & rSettingNames  =
		pNetworkData->rSettingNames();

	for (int i = 0; i < this->lnumberSettings; i++)
	{
		if (this == pSelectedVariable && this->lstepType == i)
		{
			this->lsettingRateScores[rSettingNames[i]] +=
				1.0 / this->lsettingRates[i];
		}
		this->lsettingRateScores[rSettingNames[i]] -= this->totalRate() * tau /
			this->lsettingRates[i];
	}

	// Update scores for covariate dependent rate parameters

	for (std::map<const ConstantCovariate *, double>::iterator iter =
			this->lconstantCovariateScores.begin();
		iter != this->lconstantCovariateScores.end();
		iter++)
	{
		const ConstantCovariate * pCovariate = iter->first;

		if (this == pSelectedVariable)
		{
			iter->second += pCovariate->value(selectedActor);
		}

		iter->second -= this->lconstantCovariateSumTerm[pCovariate] * tau;
	}

	for (std::map<const ChangingCovariate *, double>::iterator iter =
			this->lchangingCovariateScores.begin();
		iter != this->lchangingCovariateScores.end();
		iter++)
	{
		const ChangingCovariate * pCovariate = iter->first;
		if (this == pSelectedVariable)
		{
			iter->second += pCovariate->value(selectedActor, this->period());
		}

		iter->second -= this->lchangingCovariateSumTerm[pCovariate] * tau;
	}

	for (std::map<const BehaviorVariable *, double>::iterator iter =
			this->lbehaviorVariableScores.begin();
		iter != this->lbehaviorVariableScores.end();
		iter++)
	{
		const BehaviorVariable * pBehavior = iter->first;

		if (this == pSelectedVariable)
		{
			iter->second += pBehavior->value(selectedActor);
		}

		iter->second -= this->lbehaviorVariableSumTerm[pBehavior] * tau;
	}

	// Update scores for structural rate parameters

	for (std::map<const NetworkVariable *, double>::iterator iter =
			this->loutDegreeScores.begin();
		iter != this->loutDegreeScores.end();
		iter++)
	{
		const Network * pNetwork = iter->first->pNetwork();

		if (this == pSelectedVariable)
		{
			iter->second += pNetwork->outDegree(selectedActor);
		}

		iter->second -= this->loutDegreeSumTerm[iter->first] * tau;
	}

	for (std::map<const NetworkVariable *, double>::iterator iter =
			this->linDegreeScores.begin();
		iter != this->linDegreeScores.end();
		iter++)
	{
		const Network * pNetwork = iter->first->pNetwork();

		if (this == pSelectedVariable)
		{
			iter->second += pNetwork->inDegree(selectedActor);
		}

		iter->second -= this->linDegreeSumTerm[iter->first] * tau;
	}

	for (std::map<const NetworkVariable *, double>::iterator iter =
			this->lreciprocalDegreeScores.begin();
		iter != this->lreciprocalDegreeScores.end();
		iter++)
	{
		const OneModeNetwork * pNetwork =
			(const OneModeNetwork *) iter->first->pNetwork();

		if (this == pSelectedVariable)
		{
			iter->second += pNetwork->reciprocalDegree(selectedActor);
		}

		iter->second -= this->lreciprocalDegreeSumTerm[iter->first] * tau;
	}

	for (std::map<const NetworkVariable *, double>::iterator iter =
			this->linverseOutDegreeScores.begin();
		iter != this->linverseOutDegreeScores.end();
		iter++)
	{
		const Network * pNetwork = iter->first->pNetwork();

		if (this == pSelectedVariable)
		{
			iter->second += invertor(pNetwork->outDegree(selectedActor));
		}

		iter->second -= this->linverseOutDegreeSumTerm[iter->first] * tau;
	}

	// Update scores for diffusion rate parameters

	const vector<EffectInfo *> & rRateEffects =
		this->pSimulation()->pModel()->rRateEffects(this->name());

	for (unsigned i = 0; i < rRateEffects.size(); i++ )
	{
		EffectInfo * pInfo = rRateEffects[i];
		string rateType = pInfo->rateType();
		string effectName = pInfo->effectName();
		string interactionName = pInfo->interactionName1();
		string interactionName2 = pInfo->interactionName2();
		const BehaviorVariable * pBehaviorVariable =
			dynamic_cast<const BehaviorVariable *>(this);

		if (rateType == "diffusion")
		{
			const NetworkVariable * pVariable;
			pVariable = dynamic_cast<const NetworkVariable *>(
				this->lpSimulation->pVariable(interactionName));

			if (!pVariable)
			{
				throw logic_error("The diffusion rate effect " +
					effectName +
					" for dependent variable " +
					this->name() +
					" refers to a non-existing network variable " +
					interactionName);
			}

			const Network * pNetwork = pVariable->pNetwork();
			if (this == pSelectedVariable)
			{
				if (interactionName2 == "")
				{
					if (effectName == "avExposure" ||
						effectName == "totExposure" ||
						effectName == "susceptAvIn" ||
						effectName == "infectIn" ||
						effectName == "infectOut")
					{
						this->ldiffusionscores[pInfo] +=
							calculateDiffusionRateEffect(pBehaviorVariable,
								pNetwork, selectedActor, effectName);
					}
					else
					{
						throw domain_error("Unexpected rate effect "
							+ effectName);
					}
				}
				else
				{
					//Covariate dependent diffusion rate effects

					const ConstantCovariate * pConstantCovariate =
						this->lpSimulation->pData()->
						pConstantCovariate(interactionName2);
					const ChangingCovariate * pChangingCovariate =
						this->lpSimulation->pData()->
						pChangingCovariate(interactionName2);

					if (effectName == "susceptAvCovar" ||
						effectName == "infectCovar")
					{
						this->ldiffusionscores[pInfo] +=
							calculateDiffusionRateEffect(pBehaviorVariable,
								pNetwork, selectedActor, effectName,
								pConstantCovariate,
								pChangingCovariate);
					}
					else
					{
						throw domain_error("Unexpected rate effect " +
							effectName);
					}
				}
			}
			this->ldiffusionscores[pInfo] -= tau *
				this->ldiffusionsumterms[pInfo];
			this->pSimulation()->score(pInfo, this->ldiffusionscores[pInfo]);

		}
	}

}

/**
 * Updates the rate score functions for this event for this variable.
 * @param[in] tau the time increment in the current step of the simulation
 * @param[in] pSelectedVariable the variable, which has been selected in
 * the current step of the simulation (0, if none is selected)
 * @param[in] selectedActor the actor, which has been selected to change
 * the selected variable, if any. If no variable has been selected, this
 * parameter is ignored.
 * @param[in] alter the alter, which has been selected to make a two-way
 * change in this variable. If no variable has been selected, this
 * parameter is ignored.
 */
void DependentVariable::accumulateRateScores(double tau,
	const DependentVariable * pSelectedVariable,
	int selectedActor, int alter)
{
	 switch (this->pSimulation()->pModel()->modelType())
	 {
	 case NOTUSED:
	 case NORMAL:
	 case AFORCE:
	 case AAGREE:
		 break;
	 case BFORCE:
	 case BAGREE:
	 case BJOINT:

		 // Update the score for the basic rate parameter

		 if (this == pSelectedVariable && this->successfulChange())
		 {
			 this->lbasicRateScore += 2.0 / this->basicRate();
		 }
		 this->lbasicRateScore -=
			 2 * this->totalRate() * tau / this->basicRate();

		 // Update scores for covariate dependent rate parameters

		 for (std::map<const ConstantCovariate *,
				  double>::iterator iter =
				  this->lconstantCovariateScores.begin();
			  iter != this->lconstantCovariateScores.end();
			  iter++)
		 {
			 const ConstantCovariate * pCovariate = iter->first;

			 if (this == pSelectedVariable && this->successfulChange())
			 {
				 iter->second += pCovariate->value(selectedActor) +
					 pCovariate->value(alter);
			 }

			 iter->second -= tau *
				 this->lconstantCovariateModelBSumTerm[pCovariate];
		 }
		 for (std::map<const ChangingCovariate *, double>::iterator iter =
				  this->lchangingCovariateScores.begin();
			  iter != this->lchangingCovariateScores.end();
			  iter++)
		 {
			 const ChangingCovariate * pCovariate = iter->first;

			 if (this == pSelectedVariable && this->successfulChange())
			 {
				 iter->second +=
						 pCovariate->value(selectedActor, this->period()) +
					 pCovariate->value(alter, this->period());
			 }

			 iter->second -= tau *
				 this->lchangingCovariateModelBSumTerm[pCovariate];
		 }
		 for (std::map<const BehaviorVariable *,
				  double>::iterator iter =
				  this->lbehaviorVariableScores.begin();
			  iter != this->lbehaviorVariableScores.end();
			  iter++)
		 {
			 const BehaviorVariable * pBehavior = iter->first;

			 if (this == pSelectedVariable && this->successfulChange())
			 {
				 iter->second += pBehavior->value(selectedActor) +
					 pBehavior->value(alter);
			 }

			 iter->second -= tau *
				 this->lbehaviorVariableModelBSumTerm[pBehavior];
		 }

		 // Update scores for structural rate parameters:
		 // only out and inverse out for model type b

		 for (std::map<const NetworkVariable *, double>::iterator iter =
				  this->loutDegreeScores.begin();
			  iter != this->loutDegreeScores.end();
			  iter++)
		 {
			 const Network * pNetwork = iter->first->pNetwork();

			 if (this == pSelectedVariable && this->successfulChange())
			 {
				 iter->second += pNetwork->outDegree(selectedActor) +
					 pNetwork->outDegree(alter);
			 }

			 iter->second -= tau * this->loutDegreeModelBSumTerm[iter->first];
		 }


		 for (std::map<const NetworkVariable *, double>::iterator iter =
				  this->linverseOutDegreeScores.begin();
			  iter != this->linverseOutDegreeScores.end();
			  iter++)
		 {
			 const Network * pNetwork = iter->first->pNetwork();

			 if (this == pSelectedVariable && this->successfulChange())
			 {
				 iter->second += invertor(pNetwork->outDegree(selectedActor))
					 + invertor(pNetwork->outDegree(alter));
			 }
			 iter->second -= tau *
				 this->linverseOutDegreeModelBSumTerm[iter->first];
		 }
	 }
}
/**
 * Calculates the rate score sum terms for this variable.
 *
 */
void DependentVariable::calculateScoreSumTerms()
{
	// calculate score sum terms for covariate dependent rate parameters

	for (std::map<const ConstantCovariate *,
			 double>::iterator iter =
			 this->lconstantCovariateScores.begin();
		 iter != this->lconstantCovariateScores.end();
		 iter++)
	{
		const ConstantCovariate * pCovariate = iter->first;

		double timesRate = 0;
		double timesRateSquared = 0;
		for (int i = 0; i < this->n(); i++)
		{
			timesRate += pCovariate->value(i) * this->lrate[i];
			if (this->symmetric() && this->pSimulation()->pModel()->modelTypeB())
			{
				timesRateSquared += pCovariate->value(i) * this->lrate[i] *
					this->lrate[i];
			}
		}
		this->lconstantCovariateSumTerm[pCovariate] = timesRate;
		this->lconstantCovariateModelBSumTerm[pCovariate] = 2 *
			(this->ltotalRate * timesRate - timesRateSquared);
	}
	for (std::map<const ChangingCovariate *, double>::iterator iter =
			 this->lchangingCovariateScores.begin();
		 iter != this->lchangingCovariateScores.end();
		 iter++)
	{
		const ChangingCovariate * pCovariate = iter->first;

		double timesRate = 0;
		double timesRateSquared = 0;
		for (int i = 0; i < this->n(); i++)
		{
			timesRate += pCovariate->value(i, this->period()) * this->lrate[i];
			if (this->symmetric() && this->pSimulation()->pModel()->modelTypeB())
			{
				timesRateSquared += pCovariate->value(i, this->period()) *
					this->lrate[i] * this->lrate[i];
			}
		}
		this->lchangingCovariateSumTerm[pCovariate] = timesRate;
		this->lchangingCovariateModelBSumTerm[pCovariate] = 2 *
			(this->ltotalRate * timesRate - timesRateSquared);
	}
	for (std::map<const BehaviorVariable *,
			 double>::iterator iter =
			 this->lbehaviorVariableScores.begin();
		 iter != this->lbehaviorVariableScores.end();
		 iter++)
	{
		const BehaviorVariable * pBehavior = iter->first;

		double timesRate = 0;
		double timesRateSquared = 0;
		for (int i = 0; i < this->n(); i++)
		{
			timesRate += pBehavior->value(i) * this->lrate[i];
			if (this->symmetric() && this->pSimulation()->pModel()->modelTypeB())
			{
				timesRateSquared += pBehavior->value(i) * this->lrate[i] *
					this->lrate[i];
			}
		}
		this->lbehaviorVariableSumTerm[pBehavior] = timesRate;
		this->lbehaviorVariableModelBSumTerm[pBehavior] = 2 *
			(this->ltotalRate * timesRate - timesRateSquared);
	}

	// Update scores for structural rate parameters. NB no in- or recip-
	// for model type B

	for (std::map<const NetworkVariable *, double>::iterator iter =
			 this->linDegreeScores.begin();
		 iter != this->linDegreeScores.end();
		 iter++)
	{
		const Network * pNetwork = iter->first->pNetwork();

		double timesRate = 0;
		for (int i = 0; i < this->n(); i++)
		{
			timesRate += pNetwork->inDegree(i) * this->lrate[i];
		}
		this->linDegreeSumTerm[iter->first] = timesRate;
	}
	for (std::map<const NetworkVariable *, double>::iterator iter =
			 this->loutDegreeScores.begin();
		 iter != this->loutDegreeScores.end();
		 iter++)
	{
		const Network * pNetwork = iter->first->pNetwork();

		double timesRate = 0;
		double timesRateSquared = 0;
		for (int i = 0; i < this->n(); i++)
		{
			timesRate += pNetwork->outDegree(i) * this->lrate[i];
			if (this->symmetric() && this->pSimulation()->pModel()->modelTypeB())
			{
				timesRateSquared += pNetwork->outDegree(i) * this->lrate[i] *
					this->lrate[i];
			}
		}
		this->loutDegreeSumTerm[iter->first] = timesRate;
		this->loutDegreeModelBSumTerm[iter->first] = 2 *
			(this->ltotalRate * timesRate - timesRateSquared);
	}

	for (std::map<const NetworkVariable *, double>::iterator iter =
			 this->lreciprocalDegreeScores.begin();
		 iter != this->lreciprocalDegreeScores.end();
		 iter++)
	{
		const OneModeNetwork * pNetwork =
			(const OneModeNetwork *) iter->first->pNetwork();

		double timesRate = 0;
		for (int i = 0; i < this->n(); i++)
		{
			timesRate += pNetwork->reciprocalDegree(i) * this->lrate[i];
		}
		this->lreciprocalDegreeSumTerm[iter->first] = timesRate;
	}

	for (std::map<const NetworkVariable *, double>::iterator iter =
			 this->linverseOutDegreeScores.begin();
		 iter != this->linverseOutDegreeScores.end();
		 iter++)
	{
		const Network * pNetwork = iter->first->pNetwork();

		double timesRate = 0;
		double timesRateSquared = 0;
		for (int i = 0; i < this->n(); i++)
		{
			timesRate += invertor(pNetwork->outDegree(i)) * this->lrate[i];
			if (this->symmetric() && this->pSimulation()->pModel()->modelTypeB())
			{
				timesRateSquared += invertor(pNetwork->outDegree(i)) *
					this->lrate[i] * this->lrate[i];
			}
		}
		this->linverseOutDegreeSumTerm[iter->first] = timesRate;
		this->linverseOutDegreeModelBSumTerm[iter->first] = 2 *
			(this->ltotalRate * timesRate - timesRateSquared);

	}

	// Update scores for diffusion rate parameters.

	const vector<EffectInfo *> & rRateEffects =
		this->pSimulation()->pModel()->rRateEffects(this->name());

	for (unsigned i = 0; i < rRateEffects.size(); i++ )
	{
		EffectInfo * pInfo = rRateEffects[i];
		string rateType = pInfo->rateType();
		string effectName = pInfo->effectName();
		string interactionName = pInfo->interactionName1();
		string interactionName2 = pInfo->interactionName2();

		const BehaviorVariable * pBehaviorVariable =
			dynamic_cast<const BehaviorVariable *>(this);

		if (rateType == "diffusion")
		{
			const NetworkVariable * pVariable;
			pVariable = dynamic_cast<const NetworkVariable *>(
				this->lpSimulation->pVariable(interactionName));

			if (!pVariable)
			{
				throw logic_error("The diffusion rate effect " +
					effectName +
					" for dependent variable " +
					this->name() +
					" refers to a non-existing network variable " +
					interactionName);
			}
			const Network * pNetwork = pVariable->pNetwork();
			double timesRate = 0;
			if (interactionName2 == "")
			{
				for (int i = 0; i < this->n(); i++)
				{
					timesRate += calculateDiffusionRateEffect(
						pBehaviorVariable, pNetwork, i, effectName) *
						this->lrate[i];
				}
			}
			else
			{
				//Covariate dependent diffusion rate effects

		   		const ConstantCovariate * pConstantCovariate =
					this->lpSimulation->pData()->
					pConstantCovariate(interactionName2);
				const ChangingCovariate * pChangingCovariate =
					this->lpSimulation->pData()->
					pChangingCovariate(interactionName2);

				for (int i = 0; i < this->n(); i++)
				{
					timesRate += calculateDiffusionRateEffect(
						pBehaviorVariable, pNetwork, i, effectName,
						pConstantCovariate,
						pChangingCovariate) *
						this->lrate[i];
				}
			}
			this->ldiffusionsumterms[pInfo] = timesRate;
		}
	}
}

/**
 * Calculates the rate score functions for this chain for this variable.
 * @param[in] activeMiniStepCount the number of non-structurally determined
 * links in the current chain for this variable.
 */

void DependentVariable::calculateMaximumLikelihoodRateScores(int
	activeMiniStepCount)
{
//	Rprintf("%d %d %f\n", this->n(), activeMiniStepCount, this->basicRate());
	this->lbasicRateScore =
		- this->n() + activeMiniStepCount / this->basicRate();
}

/**
 * Calculates the rate derivative functions for this chain for this variable.
 * @param[in] activeMiniStepCount the number of non-structurally determined
 * links in the current chain for this variable.
 */

void DependentVariable::calculateMaximumLikelihoodRateDerivatives(int
	activeMiniStepCount)
{
	this->lbasicRateDerivative =
		- activeMiniStepCount / this->basicRate()/ this->basicRate();
}

double DependentVariable::basicRateScore() const
{
	return this->lbasicRateScore;
}

double DependentVariable::basicRateDerivative() const
{
	return this->lbasicRateDerivative;
}
double DependentVariable::settingRateScore(string setting) const
{

	map<string, double>::const_iterator iter =
		this->lsettingRateScores.find(setting);
	if (iter == this->lsettingRateScores.end())
	{
		throw invalid_argument("Unknown setting.");
	}
	return iter->second;
}


double DependentVariable::constantCovariateScore(
	const ConstantCovariate * pCovariate) const
{
	map<const ConstantCovariate *, double>::const_iterator iter =
		this->lconstantCovariateScores.find(pCovariate);

	if (iter == this->lconstantCovariateScores.end())
	{
		throw invalid_argument(
			string("Unknown covariate: The given covariate rate ") +
			string("effect is not part of the model."));
	}

	return iter->second;
}


double DependentVariable::changingCovariateScore(
	const ChangingCovariate * pCovariate) const
{
	map<const ChangingCovariate *, double>::const_iterator iter =
		this->lchangingCovariateScores.find(pCovariate);
	if (iter == this->lchangingCovariateScores.end())
	{
		throw invalid_argument(
			string("Unknown covariate: The given covariate rate ") +
			string("effect is not part of the model."));
	}

	return iter->second;
}


double DependentVariable::behaviorVariableScore(
	const BehaviorVariable * pBehavior) const
{
	map<const BehaviorVariable *, double>::const_iterator iter =
		this->lbehaviorVariableScores.find(pBehavior);

	if (iter == this->lbehaviorVariableScores.end())
	{
		throw invalid_argument(
			string("Unknown behavior variable: ") +
			"The given covariate rate effect is not part of the model.");
	}

	return iter->second;

}


double DependentVariable::outDegreeScore(
	const NetworkVariable * pNetworkData) const
{
	map<const NetworkVariable *, double>::const_iterator iter =
		this->loutDegreeScores.find(pNetworkData);

	if (iter == this->loutDegreeScores.end())
	{
		throw invalid_argument(
			string("Unknown network: ") +
			"The given outdegree rate effect is not part of the model.");
	}

	return iter->second;

}


double DependentVariable::inDegreeScore(
	const NetworkVariable * pNetworkData) const
{
	map<const NetworkVariable *, double>::const_iterator iter =
		this->linDegreeScores.find(pNetworkData);

	if (iter == this->linDegreeScores.end())
	{
		throw invalid_argument(
			string("Unknown network: ") +
			"The given indegree rate effect is not part of the model.");
	}

	return iter->second;
}


double DependentVariable::reciprocalDegreeScore(
	const NetworkVariable * pNetworkData) const
{
	map<const NetworkVariable *, double>::const_iterator iter =
		this->lreciprocalDegreeScores.find(pNetworkData);

	if (iter == this->lreciprocalDegreeScores.end())
	{
		throw invalid_argument(
			string("Unknown network: ") +
			"The given reciprocal degree rate effect is not " +
			"part of the model.");
	}

	return iter->second;
}


double DependentVariable::inverseOutDegreeScore(
	const NetworkVariable * pNetworkData) const
{
	map<const NetworkVariable *, double>::const_iterator iter =
		this->linverseOutDegreeScores.find(pNetworkData);

	if (iter == this->linverseOutDegreeScores.end())
	{
		throw invalid_argument(
			string("Unknown network: ") +
			"The given inverse outdegree rate effect is not " +
			"part of the model.");
	}

	return iter->second;
}

/**
 * Calculates the value of the diffusion rate effect for the given actor.
 */
double DependentVariable::calculateDiffusionRateEffect(
	const BehaviorVariable * pBehaviorVariable,
	const Network * pNetwork,
	int i, string effectName)
{
	double response = 1;
	double totalAlterValue = 0;
	if (pNetwork->outDegree(i) > 0)
	{
		if (effectName == "avExposure")
		{
			response /= double(pNetwork->outDegree(i));
		}
		else if (effectName == "susceptAvIn")
		{
			response = double(pNetwork->inDegree(i)) /
				double(pNetwork->outDegree(i));
		}
		for (IncidentTieIterator iter = pNetwork->outTies(i);
			 iter.valid();
			 iter.next())
		{
			double alterValue = pBehaviorVariable->
				value(iter.actor());

			if (effectName == "infectIn")
			{
				alterValue *= pNetwork->inDegree(i);
			}
			else if (effectName == "infectOut")
			{
				alterValue *= pNetwork->outDegree(i);
			}

			totalAlterValue += alterValue;
		}
		totalAlterValue *= response;
	}
	return totalAlterValue;
}

/**
 * Calculates the value of the covariate dependent diffusion rate effect for
 * the given actor.
 */
double DependentVariable::calculateDiffusionRateEffect(
	const BehaviorVariable * pBehaviorVariable,
	const Network * pNetwork,
	int i, string effectName, const ConstantCovariate * pConstantCovariate,
	const ChangingCovariate * pChangingCovariate)
{
	double response = 1;
	double totalAlterValue = 0;
	if (pNetwork->outDegree(i) > 0)
	{
		if (effectName == "susceptAvCovar")
		{
			if (pConstantCovariate)
			{
				response = pConstantCovariate->value(i);
			}
			else if (pChangingCovariate)
			{
				response = pChangingCovariate->value(i, this->lperiod);
			}
			else
			{
				throw logic_error("No individual covariate found.");
			}
			response /= double(pNetwork->outDegree(i));
		}
		for (IncidentTieIterator iter = pNetwork->outTies(i);
			 iter.valid();
			 iter.next())
		{
			double alterValue = pBehaviorVariable->
				value(iter.actor());

			if (effectName == "infectCovar")
			{
				if (pConstantCovariate)
				{
					alterValue *= pConstantCovariate->value(iter.actor());
				}
				else if (pChangingCovariate)
				{
					alterValue *= pChangingCovariate->value(iter.actor(),
						this->lperiod);
				}
				else
				{
					throw logic_error("No individual covariate found.");
				}
			}
			totalAlterValue += alterValue;
		}
		totalAlterValue *= response;
	}
	return totalAlterValue;
}

// ----------------------------------------------------------------------------
// Section: Composition change
// ----------------------------------------------------------------------------

/**
 * Updates this variable when an actor becomes active.
 */
void DependentVariable::actOnJoiner(const SimulationActorSet * pActorSet,
	int actor)
{
	if (pActorSet == this->lpActorSet)
	{
		this->invalidateRates();
	}
}


/**
 * Updates this variable when an actor becomes inactive.
 */
void DependentVariable::actOnLeaver(const SimulationActorSet * pActorSet,
	int actor)
{
	if (pActorSet == this->lpActorSet)
	{
		this->invalidateRates();
	}
}


// ----------------------------------------------------------------------------
// Section: Maximum likelihood related methods
// ----------------------------------------------------------------------------

/**
 * Returns whether applying the given ministep on the current state of this
 * variable would be valid with respect to all constraints. One can disable
 * the checking of up-only and down-only conditions.
 */
bool DependentVariable::validMiniStep(const MiniStep * pMiniStep,
	bool checkUpOnlyDownOnlyConditions) const
{
	return true;
}
/**
 * Updates basic rate effect parameters.
 */

void DependentVariable::updateBasicRate(int period)
{
	this->lbasicRate =
		this->lpSimulation->pModel()->basicRateParameter(this->pData(),
			period);
}
/**
 * Updates effect parameters
 */

void DependentVariable::updateEffectParameters()
{
	// find the Evaluation effectInfos
	const vector<EffectInfo *>  rEffects=
		this->lpSimulation->pModel()->rEvaluationEffects(this->name());

	const Function * pFunction = this->pEvaluationFunction();

	for (unsigned i = 0; i < pFunction->rEffects().size(); i++)
	{
		Effect * pEffect = pFunction->rEffects()[i];
		pEffect->parameter(rEffects[i]->parameter());
	}
	// find the Endowment effectInfos
	const vector<EffectInfo *>  rEffects2=
	 this->lpSimulation->pModel()->rEndowmentEffects(this->name());

	pFunction = this->pEndowmentFunction();

	for (unsigned i = 0; i < pFunction->rEffects().size(); i++)
	{
		Effect * pEffect = pFunction->rEffects()[i];
		pEffect->parameter(rEffects2[i]->parameter());
	}

	// Update the creation effect parameters

	const vector<EffectInfo *> rCreationEffectInfos =
		this->lpSimulation->pModel()->rCreationEffects(this->name());
	pFunction = this->pCreationFunction();

	for (unsigned i = 0; i < pFunction->rEffects().size(); i++)
	{
		Effect * pEffect = pFunction->rEffects()[i];
		pEffect->parameter(rCreationEffectInfos[i]->parameter());
	}

	// find the Rate effectInfos
	const vector<EffectInfo *>  rRateEffects =
		this->lpSimulation->pModel()->rRateEffects(this->name());

	vector < StructuralRateEffect * >::iterator iter =
		this->lstructuralRateEffects.begin();
	const Data * pData = this->lpSimulation->pData();

	for (unsigned i = 0; i < rRateEffects.size(); i++)
	{
		EffectInfo * pEffectInfo = rRateEffects[i];
		string interactionName = pEffectInfo->interactionName1();
		string rateType = pEffectInfo->rateType();

		if (rateType == "covariate")
		{
			ConstantCovariate * pConstantCovariate =
				pData->pConstantCovariate(interactionName);
			ChangingCovariate * pChangingCovariate =
				pData->pChangingCovariate(interactionName);
			const BehaviorVariable * pBehaviorVariable =
				(const BehaviorVariable *)
				this->lpSimulation->pVariable(interactionName);

			if (pConstantCovariate)
			{
				this->lconstantCovariateParameters[pConstantCovariate] =
					pEffectInfo->parameter();
			}
			else if (pChangingCovariate)
			{
				this->lchangingCovariateParameters[pChangingCovariate] =
					pEffectInfo->parameter();
			}
			else if (pBehaviorVariable)
			{
				this->lbehaviorVariableParameters[pBehaviorVariable] =
					pEffectInfo->parameter();
			}
			else
			{
				throw logic_error(
					"No individual covariate named '" +
					interactionName +
					"'.");
			}
		}
		else
		{
			// assume everything is is the right order!
			StructuralRateEffect *pEffect = *iter;
			//	Rprintf("%f %f to effect \n", pEffect->parameter(),
			//		pEffectInfo->parameter());
			pEffect->parameter(pEffectInfo->parameter());
			iter++;
		}
	}

	vector < DiffusionRateEffect * >::iterator iter2 =
		this->ldiffusionRateEffects.begin();

	for (unsigned i = 0; i < rRateEffects.size(); i++)
	{
		EffectInfo * pEffectInfo = rRateEffects[i];
		string interactionName = pEffectInfo->interactionName1();
		string rateType = pEffectInfo->rateType();

		if (rateType == "diffusion")
		{
			DiffusionRateEffect *pEffect = *iter2;
			pEffect->parameter(pEffectInfo->parameter());
			iter2++;
		}
	}



}
// ----------------------------------------------------------------------------
// Section: Properties
// ----------------------------------------------------------------------------

/**
 * Returns if this is a network variable.
 */
bool DependentVariable::networkVariable() const
{
	// This method is overriden in NetworkVariable. Here we return false.
	return false;
}


/**
 * Returns if this is a behavior variable.
 */
bool DependentVariable::behaviorVariable() const
{
	// This method is overriden in BehaviorVariable. Here we return false.
	return false;
}

/**
 * Returns if this is a symmetric one mode network.
 */
bool DependentVariable::symmetric() const
{
	// This method is overridden in NetworkVariable. Here we return false.
	return false;
}

/**
 * Returns if there are any constraints on the permitted changes of this
 * variable.
 */
bool DependentVariable::constrained() const
{
	return this->pData()->upOnly(this->period()) ||
		this->pData()->downOnly(this->period());
}

/**
 * Returns the alter for this step. Only used for symmetric one mode networks.
 */
int DependentVariable::alter() const
{
	// This method is overridden in NetworkVariable. Here we return false.
	return 0;
}
/**
 * Returns whether the most recent change attempted was successful.
 */
bool DependentVariable::successfulChange() const
{
	return this->lsuccessfulChange;
}

/**
 * Stores whether the most recent change attempted was successful.
 */
void DependentVariable::successfulChange(bool success)
{
	this->lsuccessfulChange = success;
}

// ----------------------------------------------------------------------------
// Section: MH step counts
// ----------------------------------------------------------------------------

/**
 * increments the number of acceptances for the given steptype for this variable
 */
void DependentVariable::incrementAcceptances(int stepType)
{
	this->lacceptances[stepType]++;
}

/**
 * increments the number of rejections for the given steptype for this variable
 */
void DependentVariable::incrementRejections(int stepType)
{
	this->lrejections[stepType]++;
}

/**
 * increments the number of aborted steps
 * for the given steptype for this variable
 */
void DependentVariable::incrementAborts(int stepType)
{
	this->laborts[stepType]++;
}

/**
 * returns the number of accepted steps
 * for the given steptype for this variable
 */
int DependentVariable::acceptances(int stepType) const
{
	return this->lacceptances[stepType];
}
/**
 * returns the number of rejected steps
 * for the given steptype for this variable
 */
int DependentVariable::rejections(int stepType) const
{
	return this->lrejections[stepType];
}
/**
 * returns the number of aborted steps
 * for the given steptype for this variable
 */
int DependentVariable::aborts(int stepType) const
{
	return this->laborts[stepType];
}

}
