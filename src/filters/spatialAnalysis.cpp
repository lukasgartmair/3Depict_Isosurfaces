#include "spatialAnalysis.h"
#include "../xmlHelper.h"

enum
{
	KEY_STOPMODE,
	KEY_ALGORITHM,
	KEY_DISTMAX,
	KEY_NNMAX,
	KEY_NUMBINS,
	KEY_REMOVAL,
	KEY_REDUCTIONDIST,
	KEY_COLOUR,
	KEY_ENABLE_SOURCE,
	KEY_ENABLE_TARGET,
};

enum {
	ALGORITHM_DENSITY, //Local density analysis
	ALGORITHM_RDF, //Radial Distribution Function
	ALGORITHM_ENUM_END,
};

enum{
	SPATIAL_DENSITY_NEIGHBOUR,
	SPATIAL_DENSITY_RADIUS,
	SPATIAL_DENSITY_ENUM_END
};

//!Error codes
enum
{
	ABORT_ERR=1,
	INSUFFICIENT_SIZE_ERR,
};
// == NN analysis filter ==



SpatialAnalysisFilter::SpatialAnalysisFilter()
{
	algorithm=ALGORITHM_DENSITY;
	nnMax=1;
	distMax=1;
	stopMode=SPATIAL_DENSITY_NEIGHBOUR;

	//Default colour is red
	r=a=1.0f;
	g=b=0.0f;

	haveRangeParent=false;
	numBins=100;
	excludeSurface=false;
	reductionDistance=distMax;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

Filter *SpatialAnalysisFilter::cloneUncached() const
{
	SpatialAnalysisFilter *p=new SpatialAnalysisFilter;

	p->algorithm=algorithm;
	p->stopMode=stopMode;
	p->nnMax=nnMax;
	p->distMax=distMax;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t SpatialAnalysisFilter::numBytesForCache(size_t nObjects) const
{
	return nObjects*IONDATA_SIZE;
}

void SpatialAnalysisFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	//Check for range file parent
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			const RangeStreamData *r;
			r = (const RangeStreamData *)dataIn[ui];

			bool different=false;
			if(!haveRangeParent)
			{
				//well, things have changed, we didn't have a 
				//range parent before.
				different=true;
			}
			else
			{
				//OK, last time we had a range parent. Check to see 
				//if the ion names are the same. If they are, keep the 
				//current bools, iff the ion names are all the same
				unsigned int numEnabled=std::count(r->enabledIons.begin(),
							r->enabledIons.end(),1);
				if(ionNames.size() == numEnabled)
				{
					unsigned int pos=0;
					for(unsigned int uj=0;uj<r->rangeFile->getNumIons();uj++)
					{
						//Only look at parent-enabled ranges
						if(r->enabledIons[uj])
						{
							if(r->rangeFile->getName(uj) != ionNames[pos])
							{
								different=true;
								break;
							}
							pos++;
						}
					}
				}
			}
			haveRangeParent=true;

			if(different)
			{
				//OK, its different. we will have to re-assign,
				//but only allow the ranges enabled in the parent filter
				ionNames.clear();
				ionNames.reserve(r->rangeFile->getNumRanges());
				for(unsigned int uj=0;uj<r->rangeFile->getNumIons();uj++)
				{

					if(r->enabledIons[uj])
						ionNames.push_back(r->rangeFile->getName(uj));
				}

				ionSourceEnabled.resize(ionNames.size(),true);
				ionTargetEnabled.resize(ionNames.size(),true);
			}

			return;
		}
	}
	haveRangeParent=false;
}

unsigned int SpatialAnalysisFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS &&
				dataIn[ui]->getStreamType() !=STREAM_TYPE_RANGE)
				getOut.push_back(dataIn[ui]);
		}
		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);
		return 0;
	}


	//Find out how much total size we need in points vector
	size_t totalDataSize=0;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);
				totalDataSize+=d->data.size();
			}
			break;	
			default:
				break;
		}
	}

	//Nothing to do.
	if(!totalDataSize)
		return 0;

	const RangeFile *rngF=0;
	if(haveRangeParent)
	{
		//Check we actually have something to do
		if(!std::count(ionSourceEnabled.begin(),
					ionSourceEnabled.end(),true))
			return 0;
		if(!std::count(ionTargetEnabled.begin(),
					ionTargetEnabled.end(),true))
			return 0;

		rngF=getRangeFile(dataIn);
	}

	//Run the algorithm
	if(algorithm == ALGORITHM_DENSITY)
	{
		//Build monolithic point set
		//---
		vector<Point3D> p;
		p.resize(totalDataSize);

		size_t dataSize=0;
		size_t n=0;

		progress.step=1;
		progress.stepName="Collate";
		progress.maxStep=3;
		(*callback)();

		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS: 
				{
					const IonStreamData *d;
					d=((const IonStreamData *)dataIn[ui]);

					if(extendPointVector(p,d->data,
							callback,progress.filterProgress,
							dataSize))
						return ABORT_ERR;

					dataSize+=d->data.size();
				}
				break;	
				default:
					break;
			}
		}
		//---

		progress.step=2;
		progress.stepName="Build";
		if(!(*callback)())
			return ABORT_ERR;

		BoundCube treeDomain;
		treeDomain.setBounds(p);

		//Build the tree (its roughly nlogn timing, but worst case n^2)
		K3DTree kdTree;
		kdTree.setCallbackMethod(callback);
		kdTree.setProgressPointer(&(progress.filterProgress));
		
		kdTree.buildByRef(p);


		//Update progress & User interface by calling callback
		if(!(*callback)())
			return ABORT_ERR;
		p.clear(); //We don't need pts any more, as tree *is* a copy.


		//Its algorithim time!
		//----
		//Update progress stuff
		n=0;
		progress.step=3;
		progress.stepName="Analyse";
		if(!(*callback)())
			return ABORT_ERR;

		//List of points for which there was a failure
		//first entry is the point Id, second is the 
		//dataset id.
		std::list<std::pair<size_t,size_t> > badPts;
		for(size_t ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS: 
				{
					const IonStreamData *d;
					d=((const IonStreamData *)dataIn[ui]);
					IonStreamData *newD = new IonStreamData;

					//Adjust this number to provide more update thanusual, because we
					//are not doing an o(1) task between updates; yes, it is  hack
					unsigned int curProg=NUM_CALLBACK/(10*nnMax);
					newD->data.resize(d->data.size());
					if(stopMode == SPATIAL_DENSITY_NEIGHBOUR)
					{
						bool spin=false;
						#pragma omp parallel for shared(spin)
						for(size_t uj=0;uj<d->data.size();uj++)
						{
							if(spin)
								continue;
							Point3D r;
							vector<const Point3D *> res;
							r=d->data[uj].getPosRef();
							
							//Assign the mass to charge using nn density estimates
							kdTree.findKNearest(r,treeDomain,nnMax,res);

							if(res.size())
							{	
								float maxSqrRad;

								//Get the radius as the furtherst object
								maxSqrRad= (res[res.size()-1]->sqrDist(r));

								//Set the mass as the volume of sphere * the number of NN
								newD->data[uj].setMassToCharge(res.size()/(4.0/3.0*M_PI*powf(maxSqrRad,3.0/2.0)));
								//Keep original position
								newD->data[uj].setPos(r);
							}
							else
							{
								#pragma omp critical
								badPts.push_back(make_pair(uj,ui));
							}

							res.clear();
							
							//Update callback as needed
							if(!curProg--)
							{
								#pragma omp critical 
								{
								n+=NUM_CALLBACK/(nnMax);
								progress.filterProgress= (unsigned int)(((float)n/(float)totalDataSize)*100.0f);
								if(!(*callback)())
									spin=true;
								curProg=NUM_CALLBACK/(nnMax);
								}
							}
						}

						if(spin)
						{
							delete newD;
							return ABORT_ERR;
						}


					}
					else if(stopMode == SPATIAL_DENSITY_RADIUS)
					{
#ifdef _OPENMP
						bool spin=false;
#endif
						float maxSqrRad = distMax*distMax;
						float vol = 4.0/3.0*M_PI*maxSqrRad*distMax; //Sphere volume=4/3 Pi R^3
						#pragma omp parallel for shared(spin) private(treeDomain)
						for(size_t uj=0;uj<d->data.size();uj++)
						{
							Point3D r;
							const Point3D *res;
							float deadDistSqr;
							unsigned int numInRad;
#ifdef _OPENMP
							if(spin)
								continue;
#endif	
							r=d->data[uj].getPosRef();
							numInRad=0;
							deadDistSqr=0;

							//Assign the mass to charge using nn density estimates
							do
							{
								res=kdTree.findNearest(r,treeDomain,deadDistSqr);

								//Check to see if we found something
								if(!res)
								{
#pragma omp critical
									badPts.push_back(make_pair(uj, ui));
									break;
								}
								
								if(res->sqrDist(r) >maxSqrRad)
									break;
								numInRad++;
								//Advance ever so slightly beyond the next ion
								deadDistSqr = res->sqrDist(r)+std::numeric_limits<float>::epsilon();
								//Update callback as needed
								if(!curProg--)
								{
									progress.filterProgress= (unsigned int)((float)n/(float)totalDataSize*100.0f);
									if(!(*callback)())
									{
#ifdef _OPENMP
										spin=true;
										continue;
#else
										delete newD;
										return ABORT_ERR;
#endif
									}
									curProg=NUM_CALLBACK/(10*nnMax);
								}
							}while(true);
							
							n++;
							//Set the mass as the volume of sphere * the number of NN
							newD->data[uj].setMassToCharge(numInRad/vol);
							//Keep original position
							newD->data[uj].setPos(r);
							
						}

#ifdef _OPENMP
						if(spin)
						{
							delete newD;
							return ABORT_ERR;
						}
#endif
					}
					else
					{
						//Should not get here.
						ASSERT(false);
					}


					//move any bad points from the array to the end, then drop them
					//To do this, we have to reverse sort the array, then
					//swap the output ion vector entries with the end,
					//then do a resize.
					ComparePairFirst cmp;
					badPts.sort(cmp);
					badPts.reverse();

					//Do some swappage
					size_t pos=1;
					for(std::list<std::pair<size_t,size_t> >::iterator it=badPts.begin(); it!=badPts.end();++it)
					{
						newD->data[(*it).first]=newD->data[newD->data.size()-pos];
					}

					//Trim the tail of bad points, leaving only good points
					newD->data.resize(newD->data.size()-badPts.size());


					//Use default colours
					newD->r=d->r;
					newD->g=d->g;
					newD->b=d->b;
					newD->a=d->a;
					newD->ionSize=d->ionSize;
					newD->representationType=d->representationType;
					newD->valueType="Number Density (\\#/Vol^3)";

					//Cache result as needed
					if(cache)
					{
						newD->cached=1;
						filterOutputs.push_back(newD);
						cacheOK=true;
					}
					else
						newD->cached=0;
					getOut.push_back(newD);
				}
				break;	
				case STREAM_TYPE_RANGE: 
					//Do not propagate ranges
				break;
				default:
					getOut.push_back(dataIn[ui]);
					break;
			}
		}
		//If we have bad points, let the user know.
		if(!badPts.empty())
		{
			std::string sizeStr;
			stream_cast(sizeStr,badPts.size());
			consoleOutput.push_back(std::string("Warning,") + sizeStr + 
					" points were un-analysable. These have been dropped");

			//Print out a list of points if we can

			size_t maxPrintoutSize=std::min(badPts.size(),(size_t)200);
			list<pair<size_t,size_t> >::iterator it;
			it=badPts.begin();
			while(maxPrintoutSize--)
			{
				std::string s;
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[it->second]);

				Point3D getPos;
				getPos=	d->data[it->first].getPosRef();
				stream_cast(s,getPos);
				consoleOutput.push_back(s);
				++it;
			}

			if(badPts.size() > 200)
			{
				consoleOutput.push_back("And so on...");
			}


		}	
	}
	else if (algorithm == ALGORITHM_RDF)
	{
		progress.step=1;
		progress.stepName="Collate";
		if(excludeSurface)
			progress.maxStep=4;
		else
			progress.maxStep=3;

		K3DTree kdTree;
		kdTree.setCallbackMethod(callback);
		kdTree.setProgressPointer(&(progress.filterProgress));
		
		//Source points
		vector<Point3D> p;
		bool needSplitting;

		needSplitting=false;
		//We only need to split up the data if we hvae to 
		if(std::count(ionSourceEnabled.begin(),ionSourceEnabled.end(),true)!=ionSourceEnabled.size())
			needSplitting=true;
		if(std::count(ionTargetEnabled.begin(),ionTargetEnabled.end(),true)!=ionTargetEnabled.size())
			needSplitting=true;

		if(haveRangeParent && needSplitting)
		{
			ASSERT(ionNames.size());
			//Build monolithic point sets
			//one for targets, one for sources
			//---
			vector<Point3D> pts[2]; //0 -> Source, 1-> Target
			size_t sizeNeeded[2];
			sizeNeeded[0]=sizeNeeded[1]=0;
			//Presize arrays
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS: 
					{
						unsigned int rangeID;

						const IonStreamData *d;
						d=((const IonStreamData *)dataIn[ui]);
						rangeID=getIonstreamIonID(d,rngF);

						if(rangeID == (unsigned int)-1)
							break;

						if(ionSourceEnabled[rangeID])
							sizeNeeded[0]+=d->data.size();

						if(ionTargetEnabled[rangeID])
							sizeNeeded[1]+=d->data.size();
						break;	
					}
					default:
						break;
				}
			}

			pts[0].resize(sizeNeeded[0]);
			pts[1].resize(sizeNeeded[1]);

			//Fill arrays	
			size_t curPos[2];
			curPos[0]=curPos[1]=0;
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS: 
					{
						unsigned int rangeID;
						const IonStreamData *d;
						d=((const IonStreamData *)dataIn[ui]);
						rangeID=getIonstreamIonID(d,rngF);

						if(rangeID==(unsigned int)(-1))
							break;
						if(ionSourceEnabled[rangeID])
						{
							if(extendPointVector(pts[0],d->data,callback,
									progress.filterProgress,curPos[0]))
								return ABORT_ERR;
							curPos[0]+=d->data.size();
						}

						if(ionTargetEnabled[rangeID])
						{
							if(extendPointVector(pts[1],d->data,callback,
									progress.filterProgress,curPos[1]))
								return ABORT_ERR;

							curPos[1]+=d->data.size();
						}
						break;
					}
					default:
						break;
				}
			}
			//---

			progress.step=2;
			progress.stepName="Build";

			//Build the tree using the target ions
			//(its roughly nlogn timing, but worst case n^2)
			kdTree.buildByRef(pts[1]);
			pts[1].clear();
			
			//Remove surface points from sources if desired
			if(excludeSurface)
			{
				ASSERT(reductionDistance > 0);
				progress.step++;
				progress.stepName="Surface";


				//Take the input points, then use them
				//to compute the convex hull reduced 
				//volume. 
				vector<Point3D> returnPoints;
				GetReducedHullPts(pts[0],reductionDistance,
						returnPoints);

				pts[0].clear();
				//Forget the original points, and use the new ones
				p.swap(returnPoints);
			}
			else
				p.swap(pts[0]);

		}
		else
		{
			//Build monolithic point set
			//---
			p.resize(totalDataSize);

			size_t dataSize=0;

			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS: 
					{
						const IonStreamData *d;
						d=((const IonStreamData *)dataIn[ui]);
						if(extendPointVector(p,d->data,callback,
							progress.filterProgress,dataSize))
							return ABORT_ERR;
						dataSize+=d->data.size();
					}
					break;	
					default:
						break;
				}
			}
			//---

			progress.step=2;
			progress.stepName="Build";
			BoundCube treeDomain;
			treeDomain.setBounds(p);

			//Build the tree (its roughly nlogn timing, but worst case n^2)
			kdTree.buildByRef(p);

			//Remove surface points if desired
			if(excludeSurface)
			{
				ASSERT(reductionDistance > 0);
				progress.step++;
				progress.stepName="Surface";


				//Take the input points, then use them
				//to compute the convex hull reduced 
				//volume. 
				vector<Point3D> returnPoints;
				GetReducedHullPts(p,reductionDistance,
						returnPoints);

				//Forget the original points, and use the new ones
				p.swap(returnPoints);

			}
			
		}

		//Let us perform the desired analysis
		progress.step++;
		progress.stepName="Analyse";

		//If there is no data, there is nothing to do.
		if(!p.size() || !kdTree.nodeCount())
			return	0;
		//OK, at this point, the KD tree contains the target points
		//of interest, and the vector "p" contains the source points
		//of interest, whatever they might be.

		if (stopMode == SPATIAL_DENSITY_NEIGHBOUR)
		{
			//User is after an NN histogram analysis

			//Histogram is output as a per-NN histogram of frequency.
			vector<vector<size_t> > histogram;
			
			//Bin widths for the NN histograms (each NN hist
			//is scaled separately). The +1 is due to the tail bin
			//being the totals
			float *binWidth = new float[nnMax];


			unsigned int errCode;
			//Run the analysis
			errCode=generateNNHist(p,kdTree,nnMax,
					numBins,histogram,binWidth,
					&(progress.filterProgress),callback);
			switch(errCode)
			{
				case 0:
					break;
				case RDF_ERR_INSUFFICIENT_INPUT_POINTS:
				{
					delete[] binWidth;
					return INSUFFICIENT_SIZE_ERR;
				}
				case RDF_ABORT_FAIL:
				{
					delete[] binWidth;
					return ABORT_ERR;
				}
				default:
					ASSERT(false);
			}

			//Alright then, we have the histogram in x-{y1,y2,y3...y_n} form
			//lets make some plots shall we?
			PlotStreamData *plotData[nnMax];

			for(unsigned int ui=0;ui<nnMax;ui++)
			{
				plotData[ui] = new PlotStreamData;
				plotData[ui]->index=ui;
				plotData[ui]->parent=this;
				plotData[ui]->xLabel="Radial Distance";
				plotData[ui]->yLabel="Count";
				std::string tmpStr;
				stream_cast(tmpStr,ui+1);
				plotData[ui]->dataLabel=getUserString() + string(" " ) +tmpStr + "NN Freq.";

				//Red plot.
				plotData[ui]->r=r;
				plotData[ui]->g=g;
				plotData[ui]->b=b;
				plotData[ui]->xyData.resize(numBins);

				for(unsigned int uj=0;uj<numBins;uj++)
				{
					float dist;
					ASSERT(ui < histogram.size() && uj<histogram[ui].size());
					dist = (float)uj*binWidth[ui];
					plotData[ui]->xyData[uj] = std::make_pair(dist,
							histogram[ui][uj]);
				}

				if(cache)
				{
					plotData[ui]->cached=1;
					filterOutputs.push_back(plotData[ui]);
					cacheOK=true;
				}	
				else
				{
					plotData[ui]->cached=0;
				}
				
				getOut.push_back(plotData[ui]);
			}




		}
		else if(stopMode == SPATIAL_DENSITY_RADIUS)
		{
			unsigned int warnBiasCount=0;
			
			//Histogram is output as a histogram of frequency vs distance
			unsigned int *histogram = new unsigned int[numBins];
			for(unsigned int ui=0;ui<numBins;ui++)
				histogram[ui] =0;
		
			//User is after an RDF analysis. Run it.
			unsigned int errcode;
			errcode=generateDistHist(p,kdTree,histogram,distMax,numBins,
					warnBiasCount,&(progress.filterProgress),callback);

			if(errcode)
			{
				return ABORT_ERR;
			}

			if(warnBiasCount)
			{
				string sizeStr;
				stream_cast(sizeStr,warnBiasCount);
			
				consoleOutput.push_back(std::string("Warning, ")
						+ sizeStr + " points were unable to find neighbour points that exceeded the search radius, and thus terminated prematurely");
			}

			PlotStreamData *plotData = new PlotStreamData;

			plotData = new PlotStreamData;
			plotData->index=0;
			plotData->parent=this;
			plotData->xLabel="Radial Distance";
			plotData->yLabel="Count";
			plotData->dataLabel=getUserString() + " RDF";

			//Red plot.
			plotData->r=r;
			plotData->g=g;
			plotData->b=b;
			plotData->xyData.resize(numBins);

			for(unsigned int uj=0;uj<numBins;uj++)
			{
				float dist;
				dist = (float)uj/(float)numBins*distMax;
				plotData->xyData[uj] = std::make_pair(dist,
						histogram[uj]);
			}

			if(cache)
			{
				plotData->cached=1;
				filterOutputs.push_back(plotData);
				cacheOK=true;
			}
			else
				plotData->cached=0;	
			
			getOut.push_back(plotData);
		
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS:
					case STREAM_TYPE_RANGE: 
						//Do not propagate ranges, or ions
					break;
					default:
						getOut.push_back(dataIn[ui]);
						break;
				}
				
			}

		}
	}



	return 0;
}

void SpatialAnalysisFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	vector<pair<unsigned int,string> > choices;
	
	tmpStr="Local Density";
	choices.push_back(make_pair((unsigned int)ALGORITHM_DENSITY,tmpStr));
	tmpStr="Radial Distribution";
	choices.push_back(make_pair((unsigned int)ALGORITHM_RDF,tmpStr));

	tmpStr= choiceString(choices,algorithm);
	s.push_back(make_pair(string("Algorithm"),tmpStr));
	choices.clear();
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_ALGORITHM);


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);


	s.clear(); type.clear(); keys.clear(); 

	//Get the options for the current algorithm
	if(algorithm ==  ALGORITHM_RDF
		||  algorithm == ALGORITHM_DENSITY)
	{
		tmpStr="Fixed Neighbour Count";

		choices.push_back(make_pair((unsigned int)SPATIAL_DENSITY_NEIGHBOUR,tmpStr));
		tmpStr="Fixed Radius";
		choices.push_back(make_pair((unsigned int)SPATIAL_DENSITY_RADIUS,tmpStr));
		tmpStr= choiceString(choices,stopMode);
		s.push_back(make_pair(string("Stop Mode"),tmpStr));
		type.push_back(PROPERTY_TYPE_CHOICE);
		keys.push_back(KEY_STOPMODE);

		if(stopMode == SPATIAL_DENSITY_NEIGHBOUR)
		{
			stream_cast(tmpStr,nnMax);
			s.push_back(make_pair(string("NN Max"),tmpStr));
			type.push_back(PROPERTY_TYPE_INTEGER);
			keys.push_back(KEY_NNMAX);
		}
		else
		{
			stream_cast(tmpStr,distMax);
			s.push_back(make_pair(string("Dist Max"),tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_DISTMAX);
		}

		//Extra options for RDF
		if(algorithm == ALGORITHM_RDF)
		{
			stream_cast(tmpStr,numBins);
			s.push_back(make_pair(string("Num Bins"),tmpStr));
			type.push_back(PROPERTY_TYPE_INTEGER);
			keys.push_back(KEY_NUMBINS);

			if(excludeSurface)
				tmpStr="1";
			else
				tmpStr="0";

			s.push_back(make_pair(string("Surface Remove"),tmpStr));
			type.push_back(PROPERTY_TYPE_BOOL);
			keys.push_back(KEY_REMOVAL);
			
			if(excludeSurface)
			{
				stream_cast(tmpStr,reductionDistance);
				s.push_back(make_pair(string("Remove Dist"),tmpStr));
				type.push_back(PROPERTY_TYPE_REAL);
				keys.push_back(KEY_REDUCTIONDIST);

			}
				
			string thisCol;
			//Convert the ion colour to a hex string	
			genColString((unsigned char)(r*255),(unsigned char)(g*255),
					(unsigned char)(b*255),(unsigned char)(a*255),thisCol);

			s.push_back(make_pair(string("Plot colour "),thisCol)); 
			type.push_back(PROPERTY_TYPE_COLOUR);
			keys.push_back(KEY_COLOUR);

			if(haveRangeParent)
			{
				propertyList.data.push_back(s);
				propertyList.types.push_back(type);
				propertyList.keys.push_back(keys);
				
				s.clear();type.clear();keys.clear();

				ASSERT(ionSourceEnabled.size() == ionNames.size());
				ASSERT(ionNames.size() == ionTargetEnabled.size());

				
				string sTmp;

				if(std::count(ionSourceEnabled.begin(),
					ionSourceEnabled.end(),true) == ionSourceEnabled.size())
					sTmp="1";
				else
					sTmp="0";

				s.push_back(make_pair("Source",sTmp));
				type.push_back(PROPERTY_TYPE_BOOL);
				keys.push_back(KEY_ENABLE_SOURCE);

					
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
				{
					if(ionSourceEnabled[ui])
						sTmp="1";
					else
						sTmp="0";
					s.push_back(make_pair(ionNames[ui],sTmp));
					type.push_back(PROPERTY_TYPE_BOOL);
					//FIXME: This is a hack...
					keys.push_back(KEY_ENABLE_SOURCE*1000+ui);
				}

				propertyList.data.push_back(s);
				propertyList.types.push_back(type);
				propertyList.keys.push_back(keys);
				
				s.clear();type.clear();keys.clear();
				if(std::count(ionTargetEnabled.begin(),
					ionTargetEnabled.end(),true) == ionTargetEnabled.size())
					sTmp="1";
				else
					sTmp="0";
				
				s.push_back(make_pair("Target",sTmp));
				type.push_back(PROPERTY_TYPE_BOOL);
				keys.push_back(KEY_ENABLE_TARGET);
				
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionTargetEnabled.size();ui++)
				{
					if(ionTargetEnabled[ui])
						sTmp="1";
					else
						sTmp="0";
					s.push_back(make_pair(ionNames[ui],sTmp));
					type.push_back(PROPERTY_TYPE_BOOL);
					//FIXME: This is a hack...
					keys.push_back(KEY_ENABLE_TARGET*1000+ui);
				}

			}
		}	
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool SpatialAnalysisFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_ALGORITHM:
		{
			size_t ltmp=ALGORITHM_ENUM_END;

			std::string tmp;
			if(value == "Local Density")
				ltmp=ALGORITHM_DENSITY;
			else if( value == "Radial Distribution")
				ltmp=ALGORITHM_RDF;
			
			if(ltmp>=ALGORITHM_ENUM_END)
				return false;
			
			algorithm=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_STOPMODE:
		{
			switch(algorithm)
			{
				case ALGORITHM_DENSITY:
				case ALGORITHM_RDF:
				{
					size_t ltmp=SPATIAL_DENSITY_ENUM_END;

					std::string tmp;
					if(value == "Fixed Radius")
						ltmp=SPATIAL_DENSITY_RADIUS;
					else if (value == "Fixed Neighbour Count")
						ltmp=SPATIAL_DENSITY_NEIGHBOUR;
					
					if(ltmp>=SPATIAL_DENSITY_ENUM_END)
						return false;
					
					stopMode=ltmp;
					needUpdate=true;
					clearCache();
					break;
				}

			
				default:
					//Should know what algorithm we use.
					ASSERT(false);
				break;
			}
			break;
		}	
		case KEY_DISTMAX:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<= 0.0)
				return false;
			
			distMax=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_NNMAX:
		{
			std::string tmp;
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp==0)
				return false;
			
			nnMax=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_NUMBINS:
		{
			std::string tmp;
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp==0)
				return false;
			
			numBins=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}
		case KEY_REDUCTIONDIST:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<= 0.0)
				return false;
			
			reductionDistance=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_REMOVAL:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=excludeSurface;
			if(stripped=="1")
				excludeSurface=true;
			else
				excludeSurface=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=excludeSurface)
			{
				needUpdate=true;
				clearCache();
			}
			
			break;
		}
		case KEY_COLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != b || newR != r ||
				newG !=g || newA != a)
			{
				r=newR/255.0;
				g=newG/255.0;
				b=newB/255.0;
				a=newA/255.0;


				clearCache();
				needUpdate=true;
			}


			break;
		}
		case KEY_ENABLE_SOURCE:
		{
			ASSERT(haveRangeParent);
			bool allEnabled=true;
			for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
			{
				if(!ionSourceEnabled[ui])
				{
					allEnabled=false;
					break;
				}
			}

			//Invert the result and assign
			allEnabled=!allEnabled;
			for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
				ionSourceEnabled[ui]=allEnabled;

			needUpdate=true;
			clearCache();
			break;
		}
		case KEY_ENABLE_TARGET:
		{
			ASSERT(haveRangeParent);
			bool allEnabled=true;
			for(unsigned int ui=0;ui<ionNames.size();ui++)
			{
				if(!ionTargetEnabled[ui])
				{
					allEnabled=false;
					break;
				}
			}

			//Invert the result and assign
			allEnabled=!allEnabled;
			for(unsigned int ui=0;ui<ionNames.size();ui++)
				ionTargetEnabled[ui]=allEnabled;

			needUpdate=true;
			clearCache();
			break;
		}
		default:
		{
			ASSERT(haveRangeParent);
			//The incoming range keys are dynamically allocated to a 
			//position beyond any reasonable key. Its a hack,
			//but it works, and is entirely contained within the filter code.
			if(key >=KEY_ENABLE_SOURCE*1000 &&
				key < KEY_ENABLE_TARGET*1000)
			{
				size_t offset;
				offset = key-KEY_ENABLE_SOURCE*1000;
				
				string stripped=stripWhite(value);

				if(!(stripped == "1"|| stripped == "0"))
					return false;
				bool lastVal = ionSourceEnabled[offset]; 
				

				if(stripped=="1")
					ionSourceEnabled[offset]=true;
				else
					ionSourceEnabled[offset]=false;

				//if the result is different, the
				//cache should be invalidated
				if(lastVal!=ionSourceEnabled[offset])
				{
					needUpdate=true;
					clearCache();
				}

				

			}	
			else if ( key >=KEY_ENABLE_TARGET*1000)
			{
				size_t offset;
				offset = key-KEY_ENABLE_TARGET*1000;
				
				string stripped=stripWhite(value);

				if(!(stripped == "1"|| stripped == "0"))
					return false;
				bool lastVal = ionTargetEnabled[offset]; 
				

				if(stripped=="1")
					ionTargetEnabled[offset]=true;
				else
					ionTargetEnabled[offset]=false;

				//if the result is different, the
				//cache should be invalidated
				if(lastVal!=ionTargetEnabled[offset])
				{
					needUpdate=true;
					clearCache();
				}
			}	
			else
			{
				ASSERT(false);
			}

		}

	}	
	return true;
}


std::string  SpatialAnalysisFilter::getErrString(unsigned int code) const
{
	//Currently the only error is aborting


	switch(code)
	{
		case ABORT_ERR:
			return std::string("Spatial analysis aborted by user");
		case INSUFFICIENT_SIZE_ERR:
			return std::string("Insufficient data to complete analysis.");
		default:
			ASSERT(false);

	}

	return std::string("Bug! (Spatial analysis filter) Shouldn't see this");
}

bool SpatialAnalysisFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;
			f << tabs(depth+1) << "<algorithm value=\""<<algorithm<< "\"/>"  << endl;
			f << tabs(depth+1) << "<stopmode value=\""<<stopMode<< "\"/>"  << endl;
			f << tabs(depth+1) << "<nnmax value=\""<<nnMax<< "\"/>"  << endl;
			f << tabs(depth+1) << "<distmax value=\""<<distMax<< "\"/>"  << endl;
			f << tabs(depth+1) << "<numbins value=\""<<numBins<< "\"/>"  << endl;
			f << tabs(depth+1) << "<excludesurface value=\""<<excludeSurface<< "\"/>"  << endl;
			f << tabs(depth+1) << "<reductiondistance value=\""<<reductionDistance<< "\"/>"  << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool SpatialAnalysisFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;

	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve algorithm
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,algorithm,"algorithm","value"))
		return false;
	if(algorithm >=ALGORITHM_ENUM_END)
		return false;
	//===
	
	//Retrieve stop mode 
	//===
	if(!XMLGetNextElemAttrib(nodePtr,stopMode,"stopmode","value"))
		return false;
	if(stopMode >=SPATIAL_DENSITY_ENUM_END)
		return false;
	//===
	
	//Retrieve nnMax val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,nnMax,"nnmax","value"))
		return false;
	if(nnMax <=0)
		return false;
	//===
	
	//Retrieve distMax val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,distMax,"distmax","value"))
		return false;
	if(distMax <=0.0)
		return false;
	//===
	
	//Retrieve numBins val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,numBins,"numbins","value"))
		return false;
	if(!numBins)
		return false;
	//===
	
	//Retreive exclude surface on/off
	//===
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"excludesurface","value"))
		return false;
	//check that new value makes sense 
	if(tmpStr == "1")
		excludeSurface=true;
	else if( tmpStr == "0")
		excludeSurface=false;
	else
		return false;
	//===
	

	//Get reduction distance
	//===
	if(!XMLGetNextElemAttrib(nodePtr,reductionDistance,"reductiondistance","value"))
		return false;
	if(reductionDistance < 0.0f)
		return false;
	//===

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;
	//====

	return true;
}
