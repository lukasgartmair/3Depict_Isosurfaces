/*
 *	mass.h - Algorithms for computing mass backgrounds 
 *	Copyright (C) 2014, D Haley 

 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.

 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.

 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mass.h"
#include <common/assertion.h>

using std::vector;

//Background modes
const char *BACKGROUND_MODE_STRING[FIT_MODE_ENUM_END] = {NTRANS("None"), 	
					NTRANS("Flat TOF")};

std::string getFitError(unsigned int errMsg) 
{
	ASSERT(errMsg < BACKGROUND_PARAMS::FIT_FAIL_END); 
	const char * errorMsgs[BACKGROUND_PARAMS::FIT_FAIL_END] = {

		NTRANS("INsufficient bins to perform fit"),
		NTRANS("Insufficient counts to perform fit"),
		NTRANS("Insufficient data to perform fit"),
		NTRANS("Data did not appear to be random noise - cannot fit noise level")
	};

	return std::string(TRANS(errorMsgs[errMsg]));
}

//Make a linearly spaced histogram with the given spacings
//TODO: Less lazy implementation
void makeHistogram(const vector<float> &data, float start, 
			float end, float step, vector<float> &histVals)
{
	ASSERT(start < end);
	ASSERT(step > std::numeric_limits<float>::epsilon());

	gsl_histogram *h = gsl_histogram_alloc((end-start)/step);
	gsl_histogram_set_ranges_uniform(h,start,end);

	for(size_t ui=0; ui<data.size();ui++)
		gsl_histogram_increment(h,data[ui]);

	//Copy out data
	histVals.resize(h->n);
	for(size_t ui=0;ui<h->n; ui++)
		histVals[ui]=h->bin[ui];

	gsl_histogram_free(h);
}

unsigned int doFitBackground(const vector<const FilterStreamData*> &dataIn, 
	BACKGROUND_PARAMS &backParams)
{
	ASSERT(backParams.mode == FIT_MODE_CONST_TOF);

	vector<const IonStreamData *> ionData;
	Filter::getStreamsOfType(dataIn,ionData);

	vector<float> sqrtFiltMass;
	for(size_t ui=0;ui<ionData.size();ui++)
	{
		for(size_t uj=0;uj<ionData[ui]->data.size(); uj++)
		{
			float curMass;
			curMass=ionData[ui]->data[uj].getMassToCharge();  
			if( curMass >=backParams.massStart && curMass <= backParams.massEnd) 
			{
				sqrtFiltMass.push_back(sqrtf(curMass));
			}
		}	
	}

	//Minimum required counts per bin to have sufficient statistics
	const unsigned int MIN_REQUIRED_AVG_COUNTS=10;
	const unsigned int MIN_REQUIRED_BINS=10;

	//CHECKME : The number of bins is the same in TOF as well as in 
	// m/c space. 	
	size_t nBins = (backParams.massEnd - backParams.massStart) / backParams.binWidth;
	float filterStep = (sqrt(backParams.massEnd) - sqrt(backParams.massStart) )/ nBins; 

	//we cannot perform a test with fewer than this number of bins
	if ( nBins < MIN_REQUIRED_BINS)
		return BACKGROUND_PARAMS::FIT_FAIL_MIN_REQ_BINS;

	float averageCounts = sqrtFiltMass.size()/ (float)nBins; 
	if( averageCounts < MIN_REQUIRED_AVG_COUNTS)
		return BACKGROUND_PARAMS::FIT_FAIL_AVG_COUNTS; 

	vector<float> histogram;
	makeHistogram(sqrtFiltMass,sqrt(backParams.massStart),
			sqrt(backParams.massEnd), filterStep,histogram);	

	float andersonStat,meanVal;
	size_t undefCount;
	if(!andersonDarlingStatistic(histogram,meanVal,backParams.stdev,andersonStat, undefCount))
		//TODO: Error message regarding fit failure
		return BACKGROUND_PARAMS::FIT_FAIL_INSUFF_DATA;

	//Rejection threshold for Anderson statistic 
	// - either we didn't have enough samples,
	// - or we failed the null hypothesis test of Gaussian-ness
	// Rejection of null hypothesis at 99% confidence occurs at 1.092 [NIST].
	// we use much more than this, in case batch processing/familywise error is present
	// two slightly overlapping Gaussians can trigger at the 1.8 level
	const float STATISTIC_THRESHOLD=3.0;
	if(andersonStat > STATISTIC_THRESHOLD || undefCount == histogram.size())
		return BACKGROUND_PARAMS::FIT_FAIL_DATA_NON_GAUSSIAN;

	//Intensity PER AMU
	//backgroundIntensity= meanVal/filterStep;
	//Intensity PER BIN in TOF space
	backParams.intensity= meanVal;

	return 0;
}

#ifdef DEBUG
#include "common/mathfuncs.h"

bool testAnderson()
{
	//Generate some normal random numbers
	RandNumGen rng;
	rng.initialise(12345);
	//Test to see if they are normal.
	vector<float> data;
	data.resize(30);

	for(size_t ui=0;ui<data.size();ui++)
	{
		data[ui]=rng.genGaussDev();
	}

	//Anderson test should pass, or something is probably wrong.
	float s,meanV,stdV;
	size_t undefcount;
	if(!andersonDarlingStatistic(data,meanV,stdV,s,undefcount) || s > 2.0f)
	{
		ASSERT(false);
		return false;
	}

	//check anderson statistic
	TEST(s >=0 && s < 1.5,"Anderson gauss test statistic");

	TEST(EQ_TOLV(meanV,0.0f,0.2f),"Gaussian mean");
	TEST(EQ_TOLV(stdV,1.0f,0.2f),"Gaussian mean");

	return true;
}

bool testBackgroundFit()
{
	RandNumGen rng;
	rng.initTimer();
	//make some random data which is flat in TOF space
	// then convert to m/c space
	IonStreamData *ionData;
	
	ionData = new IonStreamData;

	const unsigned int NUM_IONS =10000;
	const float SIMULATED_INTENSITY= 100.0f;
	
	//Simulate a histogram of NUM_IONS
	// between a lower and upper limit. 
	// This is flat in TOF space, with mean intensity
	// given by NUM_IONS/NUM_BINS
	//---
	const float TOF_LIMIT[2] = { 1.0,10};	
	
	vector<float> rawData;
	ionData->data.resize(NUM_IONS);
	rawData.resize(NUM_IONS);
	for(size_t ui=0;ui<NUM_IONS; ui++)
	{
		float simTof;
		simTof = rng.genUniformDev()*(TOF_LIMIT[1]-TOF_LIMIT[0] ) + TOF_LIMIT[0];  
		ionData->data[ui]= IonHit(Point3D(0,0,0),simTof);
		rawData[ui] = simTof;	
	}

	const float BIN_STEP=0.1f;
	vector<float> histogramRes;
	makeHistogram(rawData,TOF_LIMIT[0],TOF_LIMIT[1],
		BIN_STEP,histogramRes);
	//---

	//Find the mean and std. deviation for the tof  histogram
	float meanV,stdV;
	meanAndStdev(histogramRes,meanV,stdV);

	//check that the TOF histogram's mean matches the expected value 	
	const float EXPECTED_MEAN = NUM_IONS*BIN_STEP/(TOF_LIMIT[1] - TOF_LIMIT[0]);
	TEST(meanV > 0.95*EXPECTED_MEAN &&
		meanV < EXPECTED_MEAN*1.15,"expected mean should fall (well) within anticipated bounds, but does not"); 


	//Now perform the fit in m/c space, and after, check that it matches the anticipated m/c histogram.
	//---

	//compute the mass histogram numerically
	vector<float> massData;
	massData.resize(NUM_IONS);
	for(size_t ui=0;ui<NUM_IONS;ui++)
		massData[ui] = sqrt(rawData[ui]);
	vector<float> massHist;
	
	//Recompute the bin step parameter, as the stepping in m/c space to yield 
	// the same number of bins will e radially different
	const float NBINS = ( TOF_LIMIT[1] - TOF_LIMIT[0] )/BIN_STEP;
	const float MC_BIN_STEP = (sqrt(TOF_LIMIT[1])-sqrt(TOF_LIMIT[0]))/NBINS;
	makeHistogram(massData,sqrt(TOF_LIMIT[0]),sqrt(TOF_LIMIT[1]),MC_BIN_STEP,massHist);	

	//compute fitted value analytically
	vector<float > fittedMassHist;
	fittedMassHist.resize(NBINS);
	for(size_t ui=0;ui<histogramRes.size();ui++)
	{
		float mcX;
		mcX=(float)ui*MC_BIN_STEP + sqrtf(TOF_LIMIT[0]);
		fittedMassHist[ui]= meanV/(2*mcX);
	}
	ASSERT(massHist.size() == histogramRes.size());

	//FIXME: Test appears to be broken
	WARN(false,"Test non-functional, and algorithm broken. Fixme.");
	//check that the numerical and analytical results match
	for(size_t ui=0;ui<massHist.size();ui++)
	{
		float midV;
		midV = massHist[ui] + histogramRes[ui];
		midV*=0.5f;
		float errorFraction;
		errorFraction= fabs((massHist[ui] - histogramRes[ui])/midV);
		//ASSERT(errorFraction < 0.5f);
	}	
	//---

	return true;	
 }

#endif
