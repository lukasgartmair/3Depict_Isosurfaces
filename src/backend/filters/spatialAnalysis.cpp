/*
 *	spatialAnalysis.cpp - Perform various data analysis on 3D point clouds
 *	Copyright (C) 2013, D Haley 

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
#include "spatialAnalysis.h"



#include "rdf.h"
#include "geometryHelpers.h"
#include "filterCommon.h"



enum
{
	KEY_STOPMODE,
	KEY_ALGORITHM,
	KEY_DISTMAX,
	KEY_NNMAX,
	KEY_NUMBINS,
	KEY_REMOVAL,
	KEY_REDUCTIONDIST,
	KEY_RETAIN_UPPER,
	KEY_CUTOFF,
	KEY_COLOUR,
	KEY_ENABLE_SOURCE,
	KEY_ENABLE_TARGET,
	KEY_ORIGIN,
	KEY_NORMAL,
	KEY_RADIUS
};

enum {
	ALGORITHM_DENSITY, //Local density analysis
	ALGORITHM_DENSITY_FILTER, //Local density filtering
	ALGORITHM_RDF, //Radial Distribution Function
	ALGORITHM_AXIAL_DF, //Axial Distribution Function (aka atomvicinity, sdm, 1D rdf)
	ALGORITHM_ENUM_END,
};

enum{
	STOP_MODE_NEIGHBOUR,
	STOP_MODE_RADIUS,
	STOP_MODE_ENUM_END
};

//!Error codes
enum
{
	ABORT_ERR=1,
	INSUFFICIENT_SIZE_ERR,
};
// == NN analysis filter ==


//User visible names for the different algorithms
const char *SPATIAL_ALGORITHMS[] = {
	NTRANS("Local Density"),
	NTRANS("Density Filtering"),
	NTRANS("Radial Distribution"),
	NTRANS("Axial Distribution")
	};

const char *STOP_MODES[] = {
	NTRANS("Fixed Neighbour Count"),
	NTRANS("Fixed Radius")
};

//Switch to determine if algorithms need range propagation or not
const bool WANT_RANGE_PROPAGATION[] = { false, 
					true,
					false,
					false
					};


//Default distance to use when performing axial distance computations
const float DEFAULT_AXIAL_DISTANCE = 1.0f;

template<class T>
bool xorFunc(const T a, const T b)
{
  return (a || b) && !(a && b);
}



SpatialAnalysisFilter::SpatialAnalysisFilter()
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(STOP_MODES) == STOP_MODE_ENUM_END);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(SPATIAL_ALGORITHMS) == ALGORITHM_ENUM_END);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(WANT_RANGE_PROPAGATION) == ALGORITHM_ENUM_END);
	algorithm=ALGORITHM_DENSITY;
	nnMax=1;
	distMax=1;
	stopMode=STOP_MODE_NEIGHBOUR;

	haveRangeParent=false;
	
	//Default colour is red
	r=a=1.0f;
	g=b=0.0f;

	//RDF params
	numBins=100;
	excludeSurface=false;

	//Density filtering params
	densityCutoff=1.0f;
	keepDensityUpper=true;

	reductionDistance=distMax;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

Filter *SpatialAnalysisFilter::cloneUncached() const
{
	SpatialAnalysisFilter *p=new SpatialAnalysisFilter;

	p->r=r;
	p->g=g;
	p->b=b;
	p->a=a;
	
	p->algorithm=algorithm;
	p->stopMode=stopMode;
	p->nnMax=nnMax;
	p->distMax=distMax;

	p->numBins=numBins;
	p->excludeSurface=excludeSurface;
	p->reductionDistance=reductionDistance;
	
	p->keepDensityUpper=keepDensityUpper;
	p->densityCutoff=densityCutoff;


	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;

	p->vectorParams=vectorParams;
	p->scalarParams=scalarParams;

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

			if(WANT_RANGE_PROPAGATION[algorithm])
				dataOut.push_back(dataIn[ui]);

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
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
			{
				//Only propagate ranges if we want range propagation
				if(dataIn[ui]->getStreamType() !=STREAM_TYPE_RANGE
					|| WANT_RANGE_PROPAGATION[algorithm])
					getOut.push_back(dataIn[ui]);
			}
		}
		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);


		switch(algorithm)
		{
			case ALGORITHM_AXIAL_DF:
			{
				//Create the user interaction device required for the user
				// to interact with the algorithm parameters 
				// as this is not cached at this time
				SelectionDevice<Filter> *s;
				DrawStreamData *d= new DrawStreamData;
				d->parent=this;
				d->cached=0;

				createCylinder(d,s);
				devices.push_back(s);
				getOut.push_back(d);
				break;
			}
			default:
				;
		}
		return 0;
	}


	//Find out how much total size we need in points vector
	size_t totalDataSize=numElements(dataIn,STREAM_TYPE_IONS);

	//Nothing to do, but propagate inputs
	if(!totalDataSize)
	{
		//Propagate any inputs that we don't normally block
		for(size_t ui=0;ui<dataIn.size();ui++)
		{
			if(!(dataIn[ui]->getStreamType() & getRefreshBlockMask()))
				getOut.push_back(dataIn[ui]);
		}
		return 0;
	}

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

	
	size_t result;
	
	//Run the algorithm
	switch(algorithm)
	{
		case ALGORITHM_DENSITY:
			result=algorithmDensity(progress,callback,totalDataSize,
					dataIn,getOut);
			break;
		case ALGORITHM_RDF:
			result=algorithmRDF(progress,callback,totalDataSize,
					dataIn,getOut,rngF);
			break;
		case ALGORITHM_DENSITY_FILTER:
			result=algorithmDensityFilter(progress,callback,totalDataSize,
					dataIn,getOut);
			break;
		case ALGORITHM_AXIAL_DF:
			result=algorithmAxialDf(progress,callback,totalDataSize,
					dataIn,getOut,rngF);
			break;
		default:
			ASSERT(false);
	}
	
	return result;
}

void SpatialAnalysisFilter::getProperties(FilterPropGroup &propertyList) const
{
	FilterProperty p;
	size_t curGroup=0;

	string tmpStr;
	vector<pair<unsigned int,string> > choices;

	for(unsigned int ui=0;ui<ALGORITHM_ENUM_END;ui++)
	{
		tmpStr=TRANS(SPATIAL_ALGORITHMS[ui]);
		choices.push_back(make_pair(ui,tmpStr));
	}	
	
	tmpStr= choiceString(choices,algorithm);
	p.name=TRANS("Algorithm");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Spatial analysis algorithm to use");
	p.key=KEY_ALGORITHM;
	propertyList.addProperty(p,curGroup);
	choices.clear();

	curGroup++;
	
	//Get the options for the current algorithm
	//---

	//common options between several algorithms
	if(algorithm ==  ALGORITHM_RDF
		||  algorithm == ALGORITHM_DENSITY 
		|| algorithm == ALGORITHM_DENSITY_FILTER 
		|| algorithm == ALGORITHM_AXIAL_DF)
	{
		tmpStr=TRANS(STOP_MODES[STOP_MODE_NEIGHBOUR]);

		choices.push_back(make_pair((unsigned int)STOP_MODE_NEIGHBOUR,tmpStr));
		tmpStr=TRANS(STOP_MODES[STOP_MODE_RADIUS]);
		choices.push_back(make_pair((unsigned int)STOP_MODE_RADIUS,tmpStr));
		tmpStr= choiceString(choices,stopMode);
		p.name=TRANS("Stop Mode");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_CHOICE;
		p.helpText=TRANS("Method to use to terminate algorithm when examining each point");
		p.key=KEY_STOPMODE;
		propertyList.addProperty(p,curGroup);

		if(stopMode == STOP_MODE_NEIGHBOUR)
		{
			stream_cast(tmpStr,nnMax);
			p.name=TRANS("NN Max");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Maximum number of neighbours to examine");
			p.key=KEY_NNMAX;
		}
		else
		{
			stream_cast(tmpStr,distMax);
			p.name=TRANS("Dist Max");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Maximum distance from each point for search");
			p.key=KEY_DISTMAX;
		}
		propertyList.addProperty(p,curGroup);

	}
	
	//Extra options for specific algorithms 
	switch(algorithm)
	{
		case ALGORITHM_RDF:
		{
			stream_cast(tmpStr,numBins);
			p.name=TRANS("Num Bins");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Number of bins for output 1D RDF plot");
			p.key=KEY_NUMBINS;
			propertyList.addProperty(p,curGroup);

			if(excludeSurface)
				tmpStr="1";
			else
				tmpStr="0";

			p.name=TRANS("Surface Remove");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Exclude surface as part of source to minimise bias in RDF (at cost of increased noise)");
			p.key=KEY_REMOVAL;
			propertyList.addProperty(p,curGroup);
			
			if(excludeSurface)
			{
				stream_cast(tmpStr,reductionDistance);
				p.name=TRANS("Remove Dist");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_REAL;
				p.helpText=TRANS("Minimum distance to remove from surface");
				p.key=KEY_REDUCTIONDIST;
				propertyList.addProperty(p,curGroup);

			}
				
			string thisCol;
			//Convert the ion colour to a hex string	
			genColString((unsigned char)(r*255),(unsigned char)(g*255),
					(unsigned char)(b*255),(unsigned char)(a*255),thisCol);

			p.name=TRANS("Plot colour ");
			p.data=thisCol; 
			p.type=PROPERTY_TYPE_COLOUR;
			p.helpText=TRANS("Colour of output plot");
			p.key=KEY_COLOUR;
			propertyList.addProperty(p,curGroup);

			if(haveRangeParent)
			{
				ASSERT(ionSourceEnabled.size() == ionNames.size());
				ASSERT(ionNames.size() == ionTargetEnabled.size());
				curGroup++;

				
				string sTmp;

				if(std::count(ionSourceEnabled.begin(),
					ionSourceEnabled.end(),true) == ionSourceEnabled.size())
					sTmp="1";
				else
					sTmp="0";

				p.name=TRANS("Source");
				p.data=sTmp;
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Ions to use for initiating RDF search");
				p.key=KEY_ENABLE_SOURCE;
				propertyList.addProperty(p,curGroup);

					
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
				{
					if(ionSourceEnabled[ui])
						sTmp="1";
					else
						sTmp="0";
					p.name=ionNames[ui];
					p.data=sTmp;
					p.type=PROPERTY_TYPE_BOOL;
					p.helpText=TRANS("Enable/disable ion as source");
					//FIXME: This is a hack...
					p.key=KEY_ENABLE_SOURCE*1000+ui;
					propertyList.addProperty(p,curGroup);
				}

				curGroup++;
				
				if(std::count(ionTargetEnabled.begin(),
					ionTargetEnabled.end(),true) == ionTargetEnabled.size())
					sTmp="1";
				else
					sTmp="0";
				
				p.name=TRANS("Target");
				p.data=sTmp;
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Enable/disable all ions as target");
				p.key=KEY_ENABLE_TARGET;
				propertyList.addProperty(p,curGroup);
				
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionTargetEnabled.size();ui++)
				{
					if(ionTargetEnabled[ui])
						sTmp="1";
					else
						sTmp="0";
					p.name=ionNames[ui];
					p.data=sTmp;
					p.type=PROPERTY_TYPE_BOOL;
					p.helpText=TRANS("Enable/disable this ion as target");
					//FIXME: This is a hack...
					p.key=KEY_ENABLE_TARGET*1000+ui;
					propertyList.addProperty(p,curGroup);
				}

			}
			break;
		}	
		case ALGORITHM_DENSITY_FILTER:
		{
		
			stream_cast(tmpStr,densityCutoff);	
			p.name=TRANS("Cutoff");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Remove points with local density above/below this value");
			p.key=KEY_CUTOFF;
			propertyList.addProperty(p,curGroup);
			
			
			if(keepDensityUpper)
				tmpStr="1";
			else
				tmpStr="0";

			p.name=TRANS("Retain Upper");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Retain either points with density above (enabled) or below cutoff");
			p.key=KEY_RETAIN_UPPER;
			propertyList.addProperty(p,curGroup);
			
			break;
		}
		case ALGORITHM_DENSITY:
			break;
		case ALGORITHM_AXIAL_DF:
		{
			stream_cast(tmpStr,numBins);
			p.name=TRANS("Num Bins");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Number of bins for output 1D RDF plot");
			p.key=KEY_NUMBINS;
			propertyList.addProperty(p,curGroup);
			
			string thisCol;
			//Convert the ion colour to a hex string	
			genColString((unsigned char)(r*255),(unsigned char)(g*255),
					(unsigned char)(b*255),(unsigned char)(a*255),thisCol);

			p.name=TRANS("Plot colour ");
			p.data=thisCol; 
			p.type=PROPERTY_TYPE_COLOUR;
			p.helpText=TRANS("Colour of output plot");
			p.key=KEY_COLOUR;
			propertyList.addProperty(p,curGroup);
			
			std::string str;
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			p.key=KEY_ORIGIN;
			p.name=TRANS("Origin");
			p.data=str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Position for centre of cylinder");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(str,vectorParams[1]);
			p.key=KEY_NORMAL;
			p.name=TRANS("Axis");
			p.data=str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Vector between centre and end of cylinder");
			propertyList.addProperty(p,curGroup);

			
			stream_cast(str,scalarParams[0]);
			p.key=KEY_RADIUS;
			p.name=TRANS("Radius");
			p.data= str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Radius of cylinder");
			propertyList.addProperty(p,curGroup);
			break;
		}


		default:
			ASSERT(false);
	}
	propertyList.setGroupTitle(curGroup,TRANS("Alg. Params."));
	//---
}

bool SpatialAnalysisFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_ALGORITHM:
		{
			size_t ltmp=ALGORITHM_ENUM_END;
			for(unsigned int ui=0;ui<ALGORITHM_ENUM_END;ui++)
			{
				if(value == TRANS(SPATIAL_ALGORITHMS[ui]))
				{
					ltmp=ui;
					break;
				}
			}


			
			if(ltmp>=ALGORITHM_ENUM_END)
				return false;
			
			algorithm=ltmp;
			resetParamsAsNeeded();
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_STOPMODE:
		{
			switch(algorithm)
			{
				case ALGORITHM_DENSITY:
				case ALGORITHM_DENSITY_FILTER:
				case ALGORITHM_RDF:
				case ALGORITHM_AXIAL_DF:
				{
					size_t ltmp=STOP_MODE_ENUM_END;

					for(unsigned int ui=0;ui<STOP_MODE_ENUM_END;ui++)
					{
						if(value == TRANS(STOP_MODES[ui]))
						{
							ltmp=ui;
							break;
						}
					}
					
					if(ltmp>=STOP_MODE_ENUM_END)
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
			excludeSurface=(stripped=="1");

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

				if(cacheOK)
				{
					for(size_t ui=0;ui<filterOutputs.size();ui++)
					{
						if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT)
						{
							PlotStreamData *p;
							p =(PlotStreamData*)filterOutputs[ui];

							p->r=r;
							p->g=g;
							p->b=b;
						}
					}

				}

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
		case KEY_CUTOFF:
		{
			string stripped=stripWhite(value);

			float ltmp;
			if(stream_cast(ltmp,stripped))
				return false;

			if(ltmp<= 0.0)
				return false;
		
			if(ltmp != densityCutoff)
			{	
				densityCutoff=ltmp;
				needUpdate=true;
				clearCache();
			}
			else
				needUpdate=false;
			break;
		}
		case KEY_RETAIN_UPPER:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=keepDensityUpper;
			keepDensityUpper=(stripped=="1");

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=keepDensityUpper)
			{
				needUpdate=true;
				clearCache();
			}
			
			break;
		}
		case KEY_RADIUS:
		{
			float newRad;
			if(stream_cast(newRad,value))
				return false;

			if(scalarParams[0] != newRad )
			{
				scalarParams[0] = newRad;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_NORMAL:
		{
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_ORIGIN:
		{
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(!(vectorParams[0] == newPt ))
			{
				vectorParams[0] = newPt;
				needUpdate=true;
				clearCache();
			}

			return true;
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
			return std::string(TRANS("Spatial analysis aborted by user"));
		case INSUFFICIENT_SIZE_ERR:
			return std::string(TRANS("Insufficient data to complete analysis."));
		default:
			ASSERT(false);

	}

	return std::string("Bug! (Spatial analysis filter) Shouldn't see this");
}

void SpatialAnalysisFilter::setUserString(const std::string &str)
{
	const bool ALGORITHM_HAS_PLOTS[] = { false,false,true,true};

	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(ALGORITHM_HAS_PLOTS) == ALGORITHM_ENUM_END);

	if(userString != str && ALGORITHM_HAS_PLOTS[algorithm])
	{
		userString=str;
		clearCache();
	}	
	else
		userString=str;
}

unsigned int SpatialAnalysisFilter::getRefreshBlockMask() const
{
	//Anything but ions and ranges can go through this filter.
	if(!WANT_RANGE_PROPAGATION[algorithm])
		return STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
	else
		return STREAM_TYPE_IONS;

}

unsigned int SpatialAnalysisFilter::getRefreshEmitMask() const
{
	switch(algorithm)
	{
		case ALGORITHM_RDF:
			return STREAM_TYPE_IONS | STREAM_TYPE_PLOT;
		case ALGORITHM_AXIAL_DF:
			return STREAM_TYPE_IONS | STREAM_TYPE_PLOT | STREAM_TYPE_DRAW;
		default:
			return STREAM_TYPE_IONS;
	}
}

unsigned int SpatialAnalysisFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS;
}

bool SpatialAnalysisFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<algorithm value=\""<<algorithm<< "\"/>"  << endl;
			f << tabs(depth+1) << "<stopmode value=\""<<stopMode<< "\"/>"  << endl;
			f << tabs(depth+1) << "<nnmax value=\""<<nnMax<< "\"/>"  << endl;
			f << tabs(depth+1) << "<distmax value=\""<<distMax<< "\"/>"  << endl;
			f << tabs(depth+1) << "<numbins value=\""<<numBins<< "\"/>"  << endl;
			f << tabs(depth+1) << "<excludesurface value=\""<<excludeSurface<< "\"/>"  << endl;
			f << tabs(depth+1) << "<reductiondistance value=\""<<reductionDistance<< "\"/>"  << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;
			f << tabs(depth+1) << "<densitycutoff value=\""<<densityCutoff<< "\"/>"  << endl;
			f << tabs(depth+1) << "<keepdensityupper value=\""<<(int)keepDensityUpper<< "\"/>"  << endl;
			
			writeVectorsXML(f,"vectorparams",vectorParams,depth);
			writeScalarsXML(f,"scalarparams",scalarParams,depth);
			
			writeIonsEnabledXML(f,"source",ionSourceEnabled,ionNames,depth);
			writeIonsEnabledXML(f,"target",ionTargetEnabled,ionNames,depth);

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
	if(stopMode >=STOP_MODE_ENUM_END)
		return false;
	//===
	
	//Retrieve nnMax val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,nnMax,"nnmax","value"))
		return false;
	if(!nnMax)
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


	//Retrieve density cutoff & upper 
	if(!XMLGetNextElemAttrib(nodePtr,densityCutoff,"densitycutoff","value"))
		return false;
	if(densityCutoff< 0.0f)
		return false;

	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"keepdensityupper","value"))
		return false;
	//check that new value makes sense 
	if(tmpStr == "1")
		keepDensityUpper=true;
	else if( tmpStr == "0")
		keepDensityUpper=false;
	else
		return false;


	//FIXME: Earlier versions of the state file <= 1441:adaa3a3daa80
	// do not contain this section, so we must be fault tolerant
	// when we bin backwards compatability, do this one too.
	xmlNodePtr tmpNode;

	tmpNode=nodePtr;
	if(!XMLHelpFwdToElem(nodePtr,"scalarparams"))
		readScalarsXML(nodePtr,scalarParams);
	nodePtr=tmpNode;
	if(!XMLHelpFwdToElem(nodePtr,"vectorparams"))
		readVectorsXML(nodePtr,vectorParams);

	resetParamsAsNeeded();
	
	return true;
}

void SpatialAnalysisFilter::setPropFromBinding(const SelectionBinding &b)
{
	
	switch(b.getID())
	{
		case BINDING_CYLINDER_RADIUS:
			b.getValue(scalarParams[0]);
			break;
		case BINDING_CYLINDER_DIRECTION:
			b.getValue(vectorParams[1]);
			break;
		case BINDING_CYLINDER_ORIGIN:
			b.getValue(vectorParams[0]);
			break;
		default:
			ASSERT(false);
	}

	clearCache();
}

void SpatialAnalysisFilter::resetParamsAsNeeded()
{
	//Perform any needed
	// transformations to internal vars
	switch(algorithm)
	{
		case ALGORITHM_AXIAL_DF:
		{
			if(vectorParams.size() !=2)
			{
				size_t oldSize=vectorParams.size();
				vectorParams.resize(2);

				if(oldSize== 0)
					vectorParams[0]=Point3D(0,0,0);
				if(oldSize < 2)
					vectorParams[1]=Point3D(0,0,1);
			}

			if(scalarParams.size() !=1)
			{
				size_t oldSize=scalarParams.size();
				scalarParams.resize(1);
				if(!oldSize)
					scalarParams[0]=DEFAULT_AXIAL_DISTANCE;
			}
			break;
		}
		default:
			//fall through
		;
	}
}
//Scan input datstreams to build a two point vectors,
// one of those with points specified as "target" 
// which is a copy of the input points
//Returns 0 on no error, otherwise nonzero
size_t SpatialAnalysisFilter::buildSplitPoints(const vector<const FilterStreamData *> &dataIn,
				ProgressData &progress, size_t totalDataSize,
				const RangeFile *rngF, bool (*callback)(bool),
				vector<Point3D> &pSource, vector<Point3D> &pTarget
				) const
{
	size_t sizeNeeded[2];
	sizeNeeded[0]=sizeNeeded[1]=0;

	//Presize arrays
	for(unsigned int ui=0; ui<dataIn.size() ; ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				unsigned int ionID;

				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);
				ionID=getIonstreamIonID(d,rngF);

				if(ionID == (unsigned int)-1)
				{
					//FIXME: Fallback handling  to re-range data
					// - this can technically fail for inputs that are
					// not homogenously ranged!
					break;
				}

				if(ionSourceEnabled[ionID])
					sizeNeeded[0]+=d->data.size();

				if(ionTargetEnabled[ionID])
					sizeNeeded[1]+=d->data.size();

				break;
			}
			default:
				break;
		}
	}

	pSource.resize(sizeNeeded[0]);
	pTarget.resize(sizeNeeded[1]);

	//Fill arrays
	size_t curPos[2];
	curPos[0]=curPos[1]=0;

	for(unsigned int ui=0; ui<dataIn.size() ; ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				unsigned int ionID;
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);
				ionID=getIonstreamIonID(d,rngF);

				if(ionID==(unsigned int)(-1))
					break;

				if(ionSourceEnabled[ionID])
				{
					if(extendPointVector(pSource,d->data,callback,
					                     progress.filterProgress,curPos[0]))
						return ABORT_ERR;

					curPos[0]+=d->data.size();
				}

				if(ionTargetEnabled[ionID])
				{
					if(extendPointVector(pTarget,d->data,callback,
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

}

void SpatialAnalysisFilter::filterSelectedRanges(const vector<IonHit> &ions, bool sourceFilter, const RangeFile *rngF,
			vector<IonHit> &output) const
{
	ASSERT(rngF);

	if(sourceFilter)
	{
		for(size_t ui=0;ui<ions.size();ui++)
		{
			size_t id;
			id=rngF->getIonID(ions[ui].getMassToCharge());
			if(id == -1)
				continue;
			if(ionSourceEnabled[id])
				output.push_back(ions[ui]);
		}
	}
	else
	{
		for(size_t ui=0;ui<ions.size();ui++)
		{
			size_t id;
			id=rngF->getIonID(ions[ui].getMassToCharge());
			if(id == -1)
				continue;
			if(ionTargetEnabled[id])
				output.push_back(ions[ui]);
		}
	}
}

//Scan input datastreams to build a single point vector,
// which is a copy of the input points
//Returns 0 on no error, otherwise nonzero
size_t buildMonolithicPoints(const vector<const FilterStreamData *> &dataIn,
				ProgressData &progress, size_t totalDataSize,
				bool (*callback)(bool),	vector<Point3D> &p)
{
	//Build monolithic point set
	//---
	p.resize(totalDataSize);

	size_t dataSize=0;

	progress.filterProgress=0;
	if(!(*callback)(true))
		return ABORT_ERR;

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

	return 0;
}
			
size_t SpatialAnalysisFilter::algorithmRDF(ProgressData &progress, bool (*callback)(bool), size_t totalDataSize, 
		const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut,const RangeFile *rngF)
{
	progress.step=1;
	progress.stepName=TRANS("Collate");
	progress.filterProgress=0;
	if(excludeSurface)
		progress.maxStep=4;
	else
		progress.maxStep=3;
	
	if(!(*callback)(true))
		return ABORT_ERR;

	K3DTree kdTree;
	kdTree.setCallbackMethod(callback);
	kdTree.setProgressPointer(&(progress.filterProgress));
	
	//Source points
	vector<Point3D> p;
	bool needSplitting;

	needSplitting=false;
	//We only need to split up the data if we have to 
	if(std::count(ionSourceEnabled.begin(),ionSourceEnabled.end(),true)!=ionSourceEnabled.size()
		|| std::count(ionTargetEnabled.begin(),ionTargetEnabled.end(),true)!=ionTargetEnabled.size() )
		needSplitting=true;

	if(haveRangeParent && needSplitting)
	{
		vector<Point3D> pts[2];
		ASSERT(ionNames.size());
		size_t errCode;
		if((errCode=buildSplitPoints(dataIn,progress,totalDataSize,
				rngF,callback,pts[0],pts[1])))
			return errCode;

		progress.step=2;
		progress.stepName=TRANS("Build");

		//Build the tree using the target ions
		//(its roughly nlogn timing, but worst case n^2)
		kdTree.buildByRef(pts[1]);
		pts[1].clear();
		
		//Remove surface points from sources if desired
		if(excludeSurface)
		{
			ASSERT(reductionDistance > 0);
			progress.step++;
			progress.stepName=TRANS("Surface");

			if(!(*callback)(true))
				return ABORT_ERR;


			//Take the input points, then use them
			//to compute the convex hull reduced 
			//volume. 
			vector<Point3D> returnPoints;
			if(GetReducedHullPts(pts[0],reductionDistance,
					&progress.filterProgress,callback,returnPoints))
			{
				if(errCode ==1)
					return INSUFFICIENT_SIZE_ERR;
				else if(errCode ==2)
					return ABORT_ERR;
				else
				{
					ASSERT(false);
					return ABORT_ERR;
				}
			}
			
			if(!(*callback)(true))
				return ABORT_ERR;

			pts[0].clear();
			//Forget the original points, and use the new ones
			p.swap(returnPoints);
		}
		else
			p.swap(pts[0]);

	}
	else
	{
		size_t errCode;
		if((errCode=buildMonolithicPoints(dataIn,progress,totalDataSize,callback,p)))
			return errCode;
		
		progress.step=2;
		progress.stepName=TRANS("Build");
		BoundCube treeDomain;
		treeDomain.setBounds(p);

		//Build the tree (its roughly nlogn timing, but worst case n^2)
		kdTree.buildByRef(p);

		//Remove surface points if desired
		if(excludeSurface)
		{
			ASSERT(reductionDistance > 0);
			progress.step++;
			progress.stepName=TRANS("Surface");
		
			if(!(*callback)(true))
				return ABORT_ERR;


			//Take the input points, then use them
			//to compute the convex hull reduced 
			//volume. 
			vector<Point3D> returnPoints;
			size_t errCode;
			if(errCode=GetReducedHullPts(p,reductionDistance,
					&progress.filterProgress,callback,
					returnPoints))
			{
				if(errCode ==1)
					return INSUFFICIENT_SIZE_ERR;
				else if(errCode ==2)
					return ABORT_ERR;
				else
				{
					ASSERT(false);
					return ABORT_ERR;
				}
			}



			//Forget the original points, and use the new ones
			p.swap(returnPoints);
			
			if(!(*callback)(true))
				return ABORT_ERR;

		}
		
	}

	//Let us perform the desired analysis
	progress.step++;
	progress.stepName=TRANS("Analyse");

	//If there is no data, there is nothing to do.
	if(p.empty() || !kdTree.nodeCount())
		return	0;
	
	//OK, at this point, the KD tree contains the target points
	//of interest, and the vector "p" contains the source points
	//of interest, whatever they might be.
	switch(stopMode)
	{
		case STOP_MODE_NEIGHBOUR:
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
				plotData[ui]->plotMode=PLOT_MODE_1D;
				plotData[ui]->xLabel=TRANS("Radial Distance");
				plotData[ui]->yLabel=TRANS("Count");
				std::string tmpStr;
				stream_cast(tmpStr,ui+1);
				plotData[ui]->dataLabel=getUserString() + string(" ") +tmpStr + TRANS("NN Freq.");

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

			delete[] binWidth;
			break;
		}
		case STOP_MODE_RADIUS:
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
				return ABORT_ERR;

			if(warnBiasCount)
			{
				string sizeStr;
				stream_cast(sizeStr,warnBiasCount);
			
				consoleOutput.push_back(std::string(TRANS("Warning, "))
						+ sizeStr + TRANS(" points were unable to find neighbour points that exceeded the search radius, and thus terminated prematurely"));
			}

			PlotStreamData *plotData = new PlotStreamData;

			plotData->plotMode=PLOT_MODE_1D;
			plotData->index=0;
			plotData->parent=this;
			plotData->xLabel=TRANS("Radial Distance");
			plotData->yLabel=TRANS("Count");
			plotData->dataLabel=getUserString() + TRANS(" RDF");

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

			delete[] histogram;

			if(cache)
			{
				plotData->cached=1;
				filterOutputs.push_back(plotData);
				cacheOK=true;
			}
			else
				plotData->cached=0;	
			
			getOut.push_back(plotData);

			//Propagate non-ion/range data
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

			break;
		}
		default:
			ASSERT(false);
	}

	return 0;
}

size_t SpatialAnalysisFilter::algorithmDensity(ProgressData &progress, 
		bool (*callback)(bool), size_t totalDataSize, 
		const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut)
{
	vector<Point3D> p;
	size_t errCode;
	progress.step=1;
	progress.stepName=TRANS("Collate");
	progress.maxStep=3;
	if((errCode=buildMonolithicPoints(dataIn,progress,totalDataSize,callback,p)))
		return errCode;

	progress.step=2;
	progress.stepName=TRANS("Build");
	progress.filterProgress=0;
	if(!(*callback)(true))
		return ABORT_ERR;

	BoundCube treeDomain;
	treeDomain.setBounds(p);

	//Build the tree (its roughly nlogn timing, but worst case n^2)
	K3DTree kdTree;
	kdTree.setCallbackMethod(callback);
	kdTree.setProgressPointer(&(progress.filterProgress));
	
	kdTree.buildByRef(p);
	p.clear(); //We don't need pts any more, as tree *is* a copy.


	//Update progress & User interface by calling callback
	if(!(*callback)(false))
		return ABORT_ERR;


	//Its algorithm time!
	//----
	//Update progress stuff
	size_t n=0;
	progress.step=3;
	progress.stepName=TRANS("Analyse");
	progress.filterProgress=0;
	if(!(*callback)(true))
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
				newD->parent=this;

				//Adjust this number to provide more update than usual, because we
				//are not doing an o(1) task between updates; yes, it is a hack
				unsigned int curProg=NUM_CALLBACK/(10*nnMax);
				newD->data.resize(d->data.size());
				if(stopMode == STOP_MODE_NEIGHBOUR)
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

							//Get the radius as the furthest object
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
							if(!(*callback)(false))
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
				else if(stopMode == STOP_MODE_RADIUS)
				{
#ifdef _OPENMP
					bool spin=false;
#endif
					float maxSqrRad = distMax*distMax;
					float vol = 4.0/3.0*M_PI*maxSqrRad*distMax; //Sphere volume=4/3 Pi R^3
					#pragma omp parallel for shared(spin) firstprivate(treeDomain,curProg)
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
#pragma omp critical
								{
								progress.filterProgress= (unsigned int)((float)n/(float)totalDataSize*100.0f);
								if(!(*callback)(false))
								{
#ifdef _OPENMP
									spin=true;
#else
									delete newD;
									return ABORT_ERR;
#endif
								}
								}
#ifdef _OPENMP
								if(spin)
									break;
#endif
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


				if(newD->data.size())
				{
					//Use default colours
					newD->r=d->r;
					newD->g=d->g;
					newD->b=d->b;
					newD->a=d->a;
					newD->ionSize=d->ionSize;
					newD->representationType=d->representationType;
					newD->valueType=TRANS("Number Density (\\#/Vol^3)");

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
			}
			break;	
			case STREAM_TYPE_RANGE: 
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
		consoleOutput.push_back(std::string(TRANS("Warning,")) + sizeStr + 
				TRANS(" points were un-analysable. These have been dropped"));

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
			consoleOutput.push_back(TRANS("And so on..."));
		}


	}	
	
	return 0;
}

size_t SpatialAnalysisFilter::algorithmDensityFilter(ProgressData &progress, 
		bool (*callback)(bool), size_t totalDataSize, 
		const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut)
{
	vector<Point3D> p;
	size_t errCode;
	progress.step=1;
	progress.stepName=TRANS("Collate");
	progress.maxStep=3;
	if((errCode=buildMonolithicPoints(dataIn,progress,totalDataSize,callback,p)))
		return errCode;

	progress.step=2;
	progress.stepName=TRANS("Build");
	progress.filterProgress=0;
	if(!(*callback)(true))
		return ABORT_ERR;

	BoundCube treeDomain;
	treeDomain.setBounds(p);

	//Build the tree (its roughly nlogn timing, but worst case n^2)
	K3DTree kdTree;
	kdTree.setCallbackMethod(callback);
	kdTree.setProgressPointer(&(progress.filterProgress));
	
	kdTree.buildByRef(p);


	//Update progress & User interface by calling callback
	if(!(*callback)(false))
		return ABORT_ERR;
	p.clear(); //We don't need pts any more, as tree *is* a copy.


	//Its algorithim time!
	//----
	//Update progress stuff
	size_t n=0;
	progress.step=3;
	progress.stepName=TRANS("Analyse");
	progress.filterProgress=0;
	if(!(*callback)(true))
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
				newD->parent=this;

				//Adjust this number to provide more update thanusual, because we
				//are not doing an o(1) task between updates; yes, it is a hack
				unsigned int curProg=NUM_CALLBACK/(10*nnMax);
				newD->data.reserve(d->data.size());
				if(stopMode == STOP_MODE_NEIGHBOUR)
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


							float density;
							density = res.size()/(4.0/3.0*M_PI*powf(maxSqrRad,3.0/2.0));

							if(xorFunc((density <=densityCutoff), keepDensityUpper))
							{
#pragma omp critical
								newD->data.push_back(d->data[uj]);
							}

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
							if(!(*callback)(false))
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
				else if(stopMode == STOP_MODE_RADIUS)
				{
#ifdef _OPENMP
					bool spin=false;
#endif
					float maxSqrRad = distMax*distMax;
					float vol = 4.0/3.0*M_PI*maxSqrRad*distMax; //Sphere volume=4/3 Pi R^3
					#pragma omp parallel for shared(spin) firstprivate(treeDomain,curProg)
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
#pragma omp critical
								{
								progress.filterProgress= (unsigned int)((float)n/(float)totalDataSize*100.0f);
								if(!(*callback)(false))
								{
#ifdef _OPENMP
									spin=true;
#else
									delete newD;
									return ABORT_ERR;
#endif
								}
								}
#ifdef _OPENMP
								if(spin)
									break;
#endif
								curProg=NUM_CALLBACK/(10*nnMax);
							}
						}while(true);
						
						n++;
						float density;
						density = numInRad/vol;

						if(xorFunc((density <=densityCutoff), keepDensityUpper))
						{
#pragma omp critical
							newD->data.push_back(d->data[uj]);
						}
						
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


				if(newD->data.size())
				{
					//Use default colours
					newD->r=d->r;
					newD->g=d->g;
					newD->b=d->b;
					newD->a=d->a;
					newD->ionSize=d->ionSize;
					newD->representationType=d->representationType;
					newD->valueType=TRANS("Number Density (\\#/Vol^3)");

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
			}
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
		consoleOutput.push_back(std::string(TRANS("Warning,")) + sizeStr + 
				TRANS(" points were un-analysable. These have been dropped"));

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
			consoleOutput.push_back(TRANS("And so on..."));
		}


	}	
	
	return 0;
}

void SpatialAnalysisFilter::createCylinder(DrawStreamData * &drawData,
		SelectionDevice<Filter> * &s) const
{
	//Origin + normal
	ASSERT(vectorParams.size() == 2);
	//Add drawable components
	DrawCylinder *dC = new DrawCylinder;
	dC->setOrigin(vectorParams[0]);
	dC->setRadius(scalarParams[0]);
	dC->setColour(0.5,0.5,0.5,0.3);
	dC->setSlices(40);
	dC->setLength(sqrt(vectorParams[1].sqrMag())*2.0f);
	dC->setDirection(vectorParams[1]);
	dC->wantsLight=true;
	drawData->drawables.push_back(dC);
	
		
	//Set up selection "device" for user interaction
	//====
	//The object is selectable
	dC->canSelect=true;
	//Start and end radii must be the same (not a
	//tapered cylinder)
	dC->lockRadii();

	s = new SelectionDevice<Filter>(this);
	SelectionBinding b;
	//Bind the drawable object to the properties we wish
	//to be able to modify

	//Bind left + command button to move
	b.setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_CYLINDER_BIND_ORIGIN,
		BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
	b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
	s->addBinding(b);

	//Bind left + shift to change orientation
	b.setBinding(SELECT_BUTTON_LEFT,FLAG_SHIFT,DRAW_CYLINDER_BIND_DIRECTION,
		BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	
	b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
	s->addBinding(b);

	//Bind right button to changing position 
	b.setBinding(SELECT_BUTTON_RIGHT,0,DRAW_CYLINDER_BIND_ORIGIN,
		BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
	b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
	s->addBinding(b);
		
	//Bind middle button to changing orientation
	b.setBinding(SELECT_BUTTON_MIDDLE,0,DRAW_CYLINDER_BIND_DIRECTION,
		BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	
	b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
	s->addBinding(b);
		
	//Bind left button to changing radius
	b.setBinding(SELECT_BUTTON_LEFT,0,DRAW_CYLINDER_BIND_RADIUS,
		BINDING_CYLINDER_RADIUS,dC->getRadius(),dC);
	b.setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
	b.setFloatLimits(0,std::numeric_limits<float>::max());
	s->addBinding(b); 
	
	//=====

}

size_t SpatialAnalysisFilter::algorithmAxialDf(ProgressData &progress, 
		bool (*callback)(bool), size_t totalDataSize, 
		const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut,const RangeFile *rngF)
{
	progress.step=1;
	progress.stepName=TRANS("Extract");
	progress.filterProgress=0;
	progress.maxStep=4;

	//Ions inside the selected cylinder,
	// which are to be used as source points for dist. function query
	vector<IonHit> ionsInside;
	{
		//Crop out a cylinder as the source data 
		CropHelper cropHelp(callback,&progress.filterProgress,
				totalDataSize, CROP_CYLINDER_INSIDE,
				vectorParams,scalarParams);
				
		//Run cropping over the input datastreams
		for(size_t ui=0; ui<dataIn.size();ui++)
		{
			if( dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
			{
				const IonStreamData* d;
				d=(const IonStreamData *)dataIn[ui];
				cropHelp.runFilter(d->data,ionsInside);
			}
		}
	}

	//Now, the ions outside the targeting volume may be reduced 
	vector<IonHit> ionsOutside;
	ionsOutside.resize(totalDataSize);
	//Build complete set of input data
	{
	size_t offset=0;
	
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
			continue;


		//Copy input data in its entirety into a single vector
		const IonStreamData *d;
		d=(const IonStreamData*)dataIn[ui];
		for(size_t uj=0;uj<d->data.size();uj++)
		{
			ionsOutside[offset]=d->data[uj];
			offset++;
		}

	}
	}

	progress.step=2;
	progress.stepName=TRANS("Reduce");
	progress.filterProgress=0;

	switch(stopMode)
	{
		case STOP_MODE_RADIUS:
		{
			//In this case we can pull a small trick - 
			// do an O(n) pass to crop out the data we want, before doing the O(nlogn) tree build.
			// We however must build a slightly larger cylinder to do the crop

			vector<Point3D> vP;
			vector<float> sP;
			vP=vectorParams;
			sP=scalarParams;

			//Expand radius to encapsulate cylinder contents+dMax
			sP[0]+= distMax;
			//Similarly expand axis (vp[0] is origin, vp[1] is zis)
			vP[1].extend(distMax);

			//Crop out a cylinder as the source data 
			CropHelper cropHelp(callback,&progress.filterProgress,
					totalDataSize, CROP_CYLINDER_INSIDE,
					vP,sP);

			vector<IonHit> tmp;
			cropHelp.runFilter(ionsOutside,tmp);
			tmp.swap(ionsOutside);
			break;
		}
		case STOP_MODE_NEIGHBOUR:
		{
			//Nothing to do here!
			break;
		}
		default:
			ASSERT(false);
	}

	//We only need to slice down the data if required
	if(haveRangeParent)
	{

#pragma omp parallel
		{
		//For the ions "inside", we only care about the 
		// source data, not the target. Filter further using this info
		#pragma omp task
		{
		bool sourceReduce;
		sourceReduce=(std::count(ionSourceEnabled.begin(),ionSourceEnabled.end(),true)
						!=ionSourceEnabled.size());
		if(sourceReduce)
		{
			vector<IonHit> tmp;
			filterSelectedRanges(ionsInside,true,rngF,tmp);
			ionsInside.swap(tmp);
		}
		}
		
		#pragma omp task
		{
		bool targetReduce;
		targetReduce=(std::count(ionTargetEnabled.begin(),ionTargetEnabled.end(),true)
						!=ionTargetEnabled.size() );
		if(targetReduce)
		{
			vector<IonHit> tmp;
			filterSelectedRanges(ionsOutside,false,rngF,tmp);
			ionsOutside.swap(tmp);
		}
		}
		}
	}

	progress.step=3;
	progress.stepName=TRANS("Build");
	progress.filterProgress=0;
	
	//Strip away the real value information, leaving just point data
	vector<Point3D> src,dest;
	getPointsFromIons(ionsInside,src);
	ionsInside.clear();
	
	getPointsFromIons(ionsOutside,dest);
	ionsOutside.clear();

	K3DTree tree;
	tree.setProgressPointer(&progress.filterProgress);
	tree.setCallbackMethod(callback);
	tree.buildByRef(dest);

	progress.step=4;
	progress.stepName=TRANS("Compute");
	progress.filterProgress=0;
	unsigned int *histogram = new unsigned int[numBins];
	for(unsigned int ui=0;ui<numBins;ui++)
		histogram[ui] =0;

	float binWidth;
	//OK, so now we have two datasets that need to be analysed. Lets do it
	switch(stopMode)
	{
		case STOP_MODE_NEIGHBOUR:
		{
			unsigned int errCode;

			Point3D axisNormal=vectorParams[1];
			axisNormal.normalise();

			errCode=generate1DAxialNNHist(src,tree,axisNormal, histogram,
					binWidth,nnMax,numBins,&progress.filterProgress,callback);

			switch(errCode)
			{
				case 0:
					break;
				case RDF_ERR_INSUFFICIENT_INPUT_POINTS:
					consoleOutput.push_back(TRANS("Insufficient points to complete analysis"));
					break;
				default:
					ASSERT(false);
			}
			break;
		}
		case STOP_MODE_RADIUS:
		{
			unsigned int errCode;

			Point3D axisNormal=vectorParams[1];
			axisNormal.normalise();

			errCode=generate1DAxialDistHist(src,tree,axisNormal, histogram,
					distMax,numBins,&progress.filterProgress,callback);

			switch(errCode)
			{
				case 0:
					break;
				case RDF_ABORT_FAIL:
					return  ABORT_ERR;
				default:
					ASSERT(false);
			}
			break;
		}
		default:
			ASSERT(false);
	}


	PlotStreamData *plotData = new PlotStreamData;

	plotData->plotMode=PLOT_MODE_1D;
	plotData->index=0;
	plotData->parent=this;
	plotData->xLabel=TRANS("Axial Distance");
	plotData->yLabel=TRANS("Count");
	plotData->dataLabel=getUserString() + TRANS(" 1D Dist. Func.");

	plotData->r=r;
	plotData->g=g;
	plotData->b=b;
	plotData->xyData.resize(numBins);

	for(unsigned int uj=0;uj<numBins;uj++)
	{
		float dist;
		switch(stopMode)
		{
			case STOP_MODE_RADIUS:
				dist = ((float)uj - (float)numBins/2.0f)/(float)numBins*distMax*2.0;
				break;
			case STOP_MODE_NEIGHBOUR:
				dist= (float)uj*binWidth;
				break;
			default:
				ASSERT(false);
		}
		plotData->xyData[uj] = std::make_pair(dist,
				histogram[uj]);
	}

	delete[] histogram;
	

	if(cache)
	{
		plotData->cached=1;
		filterOutputs.push_back(plotData);
		cacheOK=true;
	}
	else
		plotData->cached=0;	
	getOut.push_back(plotData);


	//Propagate non-ion/range data
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
	
	//Create the user interaction device required for the user
	// to interact with the algorithm parameters 
	SelectionDevice<Filter> *s;
	DrawStreamData *d= new DrawStreamData;
	d->parent=this;
	d->cached=0;

	createCylinder(d,s);
	devices.push_back(s);
	getOut.push_back(d);
	return 0;
}

#ifdef DEBUG

bool densityPairTest();
bool nnHistogramTest();
bool rdfPlotTest();

bool SpatialAnalysisFilter::runUnitTests()
{
	if(!densityPairTest())
		return false;

	if(!nnHistogramTest())
		return false;

	if(!rdfPlotTest())
		return false;
	
	return true;
}


bool densityPairTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;

	

	IonStreamData*d = new IonStreamData;
	IonHit h;
	h.setMassToCharge(1);

	//create two points, 1 unit apart	
	h.setPos(Point3D(0,0,0));
	d->data.push_back(h);

	h.setPos(Point3D(0,0,1));
	d->data.push_back(h);

	streamIn.push_back(d);
	//---------
	
	//Create a spatial analysis filter
	SpatialAnalysisFilter *f=new SpatialAnalysisFilter;
	f->setCaching(false);	
	//Set it to do an NN terminated density computation
	bool needUp;
	string s;
	s=TRANS(STOP_MODES[STOP_MODE_NEIGHBOUR]);
	TEST(f->setProperty(KEY_STOPMODE,s,needUp),"Set prop");
	s=TRANS(SPATIAL_ALGORITHMS[ALGORITHM_DENSITY]);
	TEST(f->setProperty(KEY_ALGORITHM,s,needUp),"Set prop");


	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh OK");
	delete f;
	//Kill the input ion stream
	delete d; 
	streamIn.clear();

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");

	const IonStreamData* dOut = (const IonStreamData*)streamOut[0];

	TEST(dOut->data.size() == 2, "ion count");

	for(unsigned int ui=0;ui<2;ui++)
	{
		TEST( fabs( dOut->data[0].getMassToCharge()  - 1.0/(4.0/3.0*M_PI))
			< sqrt(std::numeric_limits<float>::epsilon()),"NN density test");
	}	


	delete streamOut[0];

	return true;
}

bool nnHistogramTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;

	

	IonStreamData*d = new IonStreamData;
	IonHit h;
	h.setMassToCharge(1);

	//create two points, 1 unit apart	
	h.setPos(Point3D(0,0,0));
	d->data.push_back(h);

	h.setPos(Point3D(0,0,1));
	d->data.push_back(h);

	streamIn.push_back(d);
	
	//Create a spatial analysis filter
	SpatialAnalysisFilter *f=new SpatialAnalysisFilter;
	f->setCaching(false);	
	//Set it to do an NN terminated density computation
	bool needUp;
	TEST(f->setProperty(KEY_STOPMODE,
		STOP_MODES[STOP_MODE_NEIGHBOUR],needUp),"set stop mode");
	TEST(f->setProperty(KEY_ALGORITHM,
			SPATIAL_ALGORITHMS[ALGORITHM_RDF],needUp),"set Algorithm");
	TEST(f->setProperty(KEY_NNMAX,"1",needUp),"Set NNmax");
	
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh OK");
	delete f;

	streamIn.clear();

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_PLOT,"plot outputting");
	const PlotStreamData* dPlot=(const PlotStreamData *)streamOut[0];


	float fMax=0;
	for(size_t ui=0;ui<dPlot->xyData.size();ui++)
	{
		fMax=std::max(fMax,dPlot->xyData[ui].second);
	}

	TEST(fMax > 0 , "plot has nonzero contents");
	//Kill the input ion stream
	delete d; 

	delete dPlot;

	return true;
}

bool rdfPlotTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;

	IonStreamData*d = new IonStreamData;
	IonHit h;
	h.setMassToCharge(1);

	//create two points, 1 unit apart	
	h.setPos(Point3D(0,0,0));
	d->data.push_back(h);

	h.setPos(Point3D(0,0,1));
	d->data.push_back(h);

	streamIn.push_back(d);
	
	//Create a spatial analysis filter
	SpatialAnalysisFilter *f=new SpatialAnalysisFilter;
	f->setCaching(false);	
	//Set it to do an NN terminated density computation
	bool needUp;
	TEST(f->setProperty(KEY_STOPMODE,
		TRANS(STOP_MODES[STOP_MODE_RADIUS]),needUp),"set stop mode");
	TEST(f->setProperty(KEY_ALGORITHM,
			TRANS(SPATIAL_ALGORITHMS[ALGORITHM_RDF]),needUp),"set Algorithm");
	TEST(f->setProperty(KEY_DISTMAX,"2",needUp),"Set NNmax");
	
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh OK");
	delete f;


	streamIn.clear();

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_PLOT,"plot outputting");
	const PlotStreamData* dPlot=(const PlotStreamData *)streamOut[0];


	float fMax=0;
	for(size_t ui=0;ui<dPlot->xyData.size();ui++)
	{
		fMax=std::max(fMax,dPlot->xyData[ui].second);
	}

	TEST(fMax > 0 , "plot has nonzero contents");


	//kill output data
	delete dPlot;

	//Kill the input ion stream
	delete d; 

	return true;
}


#endif

