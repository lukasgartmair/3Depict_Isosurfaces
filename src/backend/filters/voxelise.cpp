/*
 *	voxelise.cpp - Compute 3D binning (voxelisation) of point clouds
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

#include "voxelise.h"

#include "filterCommon.h"

#include <map>

enum
{
	KEY_FIXEDWIDTH,
	KEY_NBINSX,
	KEY_NBINSY,
	KEY_NBINSZ,
	KEY_WIDTHBINSX,
	KEY_WIDTHBINSY,
	KEY_WIDTHBINSZ,
	KEY_COUNT_TYPE,
	KEY_NORMALISE_TYPE,
	KEY_SPOTSIZE,
	KEY_TRANSPARANCY,
	KEY_COLOUR,
	KEY_ISOLEVEL,
	KEY_VOXEL_REPRESENTATION_MODE,
	KEY_FILTER_MODE,
	KEY_FILTER_BOUNDARY_MODE,
	KEY_FILTER_BINS,
	KEY_ENABLE_NUMERATOR,
	KEY_ENABLE_DENOMINATOR
};

//!Normalisation method
enum
{
	VOXELISE_NORMALISETYPE_NONE,// straight count
	VOXELISE_NORMALISETYPE_VOLUME,// density
	VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL, // concentration
	VOXELISE_NORMALISETYPE_COUNT2INVOXEL,// ratio count1/count2
	VOXELISE_NORMALISETYPE_MAX // keep this at the end so it's a bookend for the last value
};

//!Filtering mode
enum
{
	VOXELISE_FILTERTYPE_NONE,
	VOXELISE_FILTERTYPE_GAUSS,
	VOXELISE_FILTERTYPE_MAX // keep this at the end so it's a bookend for the last value
};


//Boundary behaviour for filtering 
enum
{
	VOXELISE_FILTERBOUNDMODE_ZERO,
	VOXELISE_FILTERBOUNDMODE_BOUNCE,
	VOXELISE_FILTERBOUNDMODE_MAX// keep this at the end so it's a bookend for the last value
};

enum
{
	VOXELISE_ABORT_ERR,
	VOXELISE_MEMORY_ERR,
	VOXELISE_CONVOLVE_ERR,
	VOXELISE_BOUNDS_INVALID_ERR
};

const char *NORMALISE_TYPE_STRING[] = {
		NTRANS("None (Raw count)"),
		NTRANS("Volume (Density)"),
		NTRANS("All Ions (conc)"),
		NTRANS("Ratio (Num/Denom)"),
	};

const char *REPRESENTATION_TYPE_STRING[] = {
		NTRANS("Point Cloud"),
		NTRANS("Isosurface")
	};

const char *VOXELISE_FILTER_TYPE_STRING[]={
	NTRANS("None"),
	NTRANS("Gaussian (2𝜎)"),
	};

const char *VOXELISE_FILTER_BOUND_STRING[] ={
	NTRANS("Zero"),
	NTRANS("Bounce")
	};

//This is not a member of voxels.h, as the voxels do not have any concept of the IonHit
int countPoints(Voxels<float> &v, const std::vector<IonHit> &points, 
				bool noWrap,bool (*callback)(bool))
{

	size_t x,y,z;
	size_t binCount[3];
	v.getSize(binCount[0],binCount[1],binCount[2]);

	unsigned int downSample=MAX_CALLBACK;
	for (size_t ui=0; ui<points.size(); ui++)
	{
		if(!downSample--)
		{
			if(!(*callback)(false))
				return 1;
			downSample=MAX_CALLBACK;
		}
		v.getIndexWithUpper(x,y,z,points[ui].getPos());
		//Ensure it lies within the dataset
		if (x < binCount[0] && y < binCount[1] && z< binCount[2])
		{
			{
				float value;
				value=v.getData(x,y,z)+1.0f;

				//Prevent wrap-around errors
				if (noWrap) {
					if (value > v.getData(x,y,z))
						v.setData(x,y,z,value);
				} else {
					v.setData(x,y,z,value);
				}
			}
		}
	}
	return 0;
}

// == Voxels filter ==
VoxeliseFilter::VoxeliseFilter() 
: fixedWidth(false), normaliseType(VOXELISE_NORMALISETYPE_NONE)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(NORMALISE_TYPE_STRING) ==  VOXELISE_NORMALISETYPE_MAX);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(VOXELISE_FILTER_TYPE_STRING) == VOXELISE_FILTERTYPE_MAX );
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(VOXELISE_FILTER_BOUND_STRING) ==  VOXELISE_FILTERBOUNDMODE_MAX);

	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(REPRESENTATION_TYPE_STRING) == VOXEL_REPRESENT_END);

	splatSize=1.0f;
	a=0.9f;
	r=g=b=0.5;
	isoLevel=0.5;
	
	filterBins=3;
	filterMode=VOXELISE_FILTERTYPE_NONE;
	filterBoundaryMode=VOXELISE_FILTERBOUNDMODE_BOUNCE;
	gaussDev=0.5;	
	
	representation=VOXEL_REPRESENT_POINTCLOUD;


	//Ficticious bounds.
	bc.setBounds(Point3D(0,0,0),Point3D(1,1,1));

	for (unsigned int i = 0; i < INDEX_LENGTH; i++) 
		nBins[i] = 50;

	calculateWidthsFromNumBins(binWidth,nBins);

	numeratorAll = false;
	denominatorAll = true;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up


	rsdIncoming=0;
}


Filter *VoxeliseFilter::cloneUncached() const
{
	VoxeliseFilter *p=new VoxeliseFilter();
	p->splatSize=splatSize;
	p->a=a;
	p->r=r;
	p->g=g;
	p->b=b;

	p->isoLevel=isoLevel;
	
	p->filterMode=filterMode;
	p->filterBoundaryMode=filterBoundaryMode;
	p->filterBins=filterBins;
	p->gaussDev=gaussDev;

	p->representation=representation;
	p->splatSize=splatSize;

	p->normaliseType=normaliseType;
	p->numeratorAll=numeratorAll;
	p->denominatorAll=denominatorAll;

	p->bc=bc;

	for(size_t ui=0;ui<INDEX_LENGTH;ui++)
	{
		p->nBins[ui] = nBins[ui];
		p->binWidth[ui] = binWidth[ui];
	}

	p->enabledIons[0].resize(enabledIons[0].size());
	std::copy(enabledIons[0].begin(),enabledIons[0].end(),p->enabledIons[0].begin());
	
	p->enabledIons[1].resize(enabledIons[1].size());
	std::copy(enabledIons[1].begin(),enabledIons[1].end(),p->enabledIons[1].begin());

	if(rsdIncoming)
	{
		p->rsdIncoming=new RangeStreamData();
		*(p->rsdIncoming) = *rsdIncoming;
	}
	else
		p->rsdIncoming=0;

	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t VoxeliseFilter::numBytesForCache(size_t nObjects) const
{
	//if we are using fixed width, we know the answer.
	//otherwise we dont until we are presented with the boundcube.
	//TODO: Modify the function description to pass in the boundcube
	if(!fixedWidth)
		return 	nBins[0]*nBins[1]*nBins[2]*sizeof(float);
	else
		return 0;
}

void VoxeliseFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
						std::vector<const FilterStreamData *> &dataOut)
{
	const RangeStreamData *c=0;
	//Determine if we have an incoming range
	for (size_t i = 0; i < dataIn.size(); i++) 
	{
		if(dataIn[i]->getStreamType() == STREAM_TYPE_RANGE)
		{
			c=(const RangeStreamData *)dataIn[i];

			break;
		}
	}

	//we no longer (or never did) have any incoming ranges. Not much to do
	if(!c)
	{
		//delete the old incoming range pointer
		if(rsdIncoming)
			delete rsdIncoming;
		rsdIncoming=0;

		enabledIons[0].clear(); //clear numerator options
		enabledIons[1].clear(); //clear denominator options

		//Prevent normalisation type being set incorrectly
		// if we have no incoming range data
		if(normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL || normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL)
			normaliseType= VOXELISE_NORMALISETYPE_NONE;
	}
	else
	{


		//If we didn't have an incoming rsd, then make one up!
		if(!rsdIncoming)
		{
			rsdIncoming = new RangeStreamData;
			*rsdIncoming=*c;

			//set the numerator to all disabled
			enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
			//set the denominator to have all enabled
			enabledIons[1].resize(rsdIncoming->rangeFile->getNumIons(),1);
		}
		else
		{

			//OK, so we have a range incoming already (from last time)
			//-- the question is, is it the same
			//one we had before 
			//Do a pointer comparison (its a hack, yes, but it should work)
			if(rsdIncoming->rangeFile != c->rangeFile)
			{
				//hmm, it is different. well, trash the old incoming rng
				delete rsdIncoming;

				rsdIncoming = new RangeStreamData;
				*rsdIncoming=*c;

				//set the numerator to all disabled
				enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
				//set the denominator to have all enabled
				enabledIons[1].resize(rsdIncoming->rangeFile->getNumIons(),1);
			}
		}

	}
}

unsigned int VoxeliseFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
		  std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
{
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		//Disallow copying of anything in the blockmask. Copy everything else
		if(!(dataIn[ui]->getStreamType() & getRefreshBlockMask() ))
			getOut.push_back(dataIn[ui]);
	}
	
	//use the cached copy if we have it.
	if(cacheOK)
	{
		for(size_t ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);
		return 0;
	}


	Point3D minP,maxP;

	bc.setInverseLimits();
		
	for (size_t i = 0; i < dataIn.size(); i++) 
	{
		//Check for ion stream types. Block others from propagation.
		if (dataIn[i]->getStreamType() != STREAM_TYPE_IONS) continue;

		const IonStreamData *is = (const IonStreamData *)dataIn[i];
		//Don't work on empty or single object streams (bounding box needs to be defined)
		if (is->getNumBasicObjects() < 2) continue;
	
		BoundCube bcTmp;
		bcTmp=getIonDataLimits(is->data);

		//Bounds could be invalid if, for example, we had coplanar axis aligned points
		if (!bcTmp.isValid()) continue;

		bc.expand(bcTmp);
	}
	//No bounding box? Tough cookies
	if (!bc.isValid() || bc.isFlat()) return VOXELISE_BOUNDS_INVALID_ERR;

	bc.getBounds(minP,maxP);	
	if (fixedWidth) 
		calculateNumBinsFromWidths(binWidth, nBins);
	else
		calculateWidthsFromNumBins(binWidth, nBins);
	
	//Disallow empty bounding boxes (ie, produce no output)
	if(minP == maxP)
		return 0;
		
	VoxelStreamData *vs = new VoxelStreamData();
	vs->parent=this;
	vs->data.setCallbackMethod(callback);
	vs->data.init(nBins[0], nBins[1], nBins[2], bc);
	vs->representationType= representation;
	vs->splatSize = splatSize;
	vs->isoLevel=isoLevel;
	vs->data.fill(0);
	vs->r=r;
	vs->g=g;
	vs->b=b;
	vs->a=a;

	Voxels<float> vsDenom;
	if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL ||
		normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL) {
		//Check we actually have incoming data
		ASSERT(rsdIncoming);
		vsDenom.setCallbackMethod(callback);
		vsDenom.init(nBins[0], nBins[1], nBins[2], bc);
		vsDenom.fill(0);
	}

	const IonStreamData *is;
	if(rsdIncoming)
	{

		for (size_t i = 0; i < dataIn.size(); i++) 
		{
			
			//Check for ion stream types. Don't use anything else in counting
			if (dataIn[i]->getStreamType() != STREAM_TYPE_IONS) continue;
			
			is= (const IonStreamData *)dataIn[i];

			
			//Count the numerator ions	
			if(is->data.size())
			{
				//Check what Ion type this stream belongs to. Assume all ions
				//in the stream belong to the same group
				unsigned int ionID;
				ionID = getIonstreamIonID(is,rsdIncoming->rangeFile);

				bool thisIonEnabled;
				if(ionID!=(unsigned int)-1)
					thisIonEnabled=enabledIons[0][ionID];
				else
					thisIonEnabled=false;

				if(thisIonEnabled)
				{
					countPoints(vs->data,is->data,true,callback);
				}
			}
		
			//If the user requests normalisation, compute the denominator dataset
			if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL) {
				if(is->data.size())
				{
					//Check what Ion type this stream belongs to. Assume all ions
					//in the stream belong to the same group
					unsigned int ionID;
					ionID = rsdIncoming->rangeFile->getIonID(is->data[0].getMassToCharge());

					bool thisIonEnabled;
					if(ionID!=(unsigned int)-1)
						thisIonEnabled=enabledIons[1][ionID];
					else
						thisIonEnabled=false;

					if(thisIonEnabled)
						countPoints(vsDenom,is->data,true,callback);
				}
			} else if (normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL)
			{
				countPoints(vsDenom,is->data,true,callback);
			}

			if(!(*callback)(false))
			{
				delete vs;
				return VOXELISE_ABORT_ERR;
			}
		}
	
		//Perform normalsiation	
		if (normaliseType == VOXELISE_NORMALISETYPE_VOLUME)
			vs->data.calculateDensity();
		else if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL ||
				 normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL)
			vs->data /= vsDenom;
	}
	else
	{
		//No range data.  Just count
		for (size_t i = 0; i < dataIn.size(); i++) 
		{
			
			if(dataIn[i]->getStreamType() == STREAM_TYPE_IONS)
			{
				is= (const IonStreamData *)dataIn[i];

				countPoints(vs->data,is->data,true,callback);
				
				if(!(*callback)(false))
				{
					delete vs;
					return VOXELISE_ABORT_ERR;
				}

			}
		}
		ASSERT(normaliseType != VOXELISE_NORMALISETYPE_COUNT2INVOXEL
				&& normaliseType!=VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL);
		if (normaliseType == VOXELISE_NORMALISETYPE_VOLUME)
			vs->data.calculateDensity();
	}	

	vsDenom.clear();


	//Perform voxel filtering
	switch(filterMode)
	{
		case VOXELISE_FILTERTYPE_NONE:
			break;
		case VOXELISE_FILTERTYPE_GAUSS:
		{
			Voxels<float> kernel,res;

			map<unsigned int, unsigned int> modeMap;


			modeMap[VOXELISE_FILTERBOUNDMODE_ZERO]=BOUND_ZERO;
			modeMap[VOXELISE_FILTERBOUNDMODE_BOUNCE]=BOUND_MIRROR;

			//FIXME: This will be SLOW. need to use IIR or some other
			//fast technique
			
			//Construct the gaussian convolution
			kernel.setGaussianKernelCube(gaussDev,(float)filterBins,filterBins);
			//Normalise the kernel
			float sum;
			sum=kernel.getSum();
			kernel/=sum;
		
			if(res.resize(vs->data))
				return VOXELISE_MEMORY_ERR; 

			//Gaussian kernel is separable (rank 1)
			if(vs->data.separableConvolve(kernel,res,modeMap[filterBoundaryMode]))
				return VOXELISE_CONVOLVE_ERR;

			vs->data.swap(res);

			res.clear();
			break;
		}
		default:
			ASSERT(false);
	}

	
	float min,max;
	vs->data.minMax(min,max);


	string sMin,sMax;
	stream_cast(sMin,min);
	stream_cast(sMax,max);
	consoleOutput.push_back(std::string(TRANS("Voxel Limits (min,max): (") + sMin + string(","))
		       	+  sMax + ")");
	if(cache)
	{
		vs->cached=1;
		cacheOK=true;
		filterOutputs.push_back(vs);
	}
	else
		vs->cached=0;


	//Store the voxels on the output
	getOut.push_back(vs);
	
	//Copy the inputs into the outputs, provided they are not voxels
	return 0;
}

std::string VoxeliseFilter::getNormaliseTypeString(int type){
	ASSERT(type < VOXELISE_NORMALISETYPE_MAX);
	return TRANS(NORMALISE_TYPE_STRING[type]);
}

std::string VoxeliseFilter::getRepresentTypeString(int type) {
	ASSERT(type<VOXEL_REPRESENT_END);
	return  std::string(TRANS(REPRESENTATION_TYPE_STRING[type]));
}

std::string VoxeliseFilter::getFilterTypeString(int type)
{
	ASSERT(type < VOXELISE_FILTERTYPE_MAX);
	return std::string(TRANS(VOXELISE_FILTER_TYPE_STRING[type]));
}


std::string VoxeliseFilter::getFilterBoundTypeString(int type) 
{
	ASSERT(type < VOXELISE_FILTERBOUNDMODE_MAX);
	return std::string(TRANS(VOXELISE_FILTER_BOUND_STRING[type]));
}

void VoxeliseFilter::getProperties(FilterPropGroup &propertyList) const
{
	FilterProperty p;
	size_t curGroup=0;

	string tmpStr;
	stream_cast(tmpStr, fixedWidth);
	p.name=TRANS("Fixed width");
	p.data=tmpStr;
	p.key=KEY_FIXEDWIDTH;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("If true, use fixed size voxels, otherwise use fixed count");
	propertyList.addProperty(p,curGroup);

	if(fixedWidth)
	{
		stream_cast(tmpStr,binWidth[0]);
		p.name=TRANS("Bin width x");
		p.data=tmpStr;
		p.key=KEY_WIDTHBINSX;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Voxel size in X direction");
		propertyList.addProperty(p,curGroup);

		stream_cast(tmpStr,binWidth[1]);
		p.name=TRANS("Bin width y");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Voxel size in Y direction");
		p.key=KEY_WIDTHBINSY;
		propertyList.addProperty(p,curGroup);


		stream_cast(tmpStr,binWidth[2]);
		p.name=TRANS("Bin width z");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Voxel size in Z direction");
		p.key=KEY_WIDTHBINSZ;
		propertyList.addProperty(p,curGroup);
	}
	else
	{
		stream_cast(tmpStr,nBins[0]);
		p.name=TRANS("Num bins x");
		p.data=tmpStr;
		p.key=KEY_NBINSX;
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of voxels to use in X direction");
		propertyList.addProperty(p,curGroup);
		
		stream_cast(tmpStr,nBins[1]);
		p.key=KEY_NBINSY;
		p.name=TRANS("Num bins y");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of voxels to use in Y direction");
		propertyList.addProperty(p,curGroup);
		
		stream_cast(tmpStr,nBins[2]);
		p.key=KEY_NBINSZ;
		p.data=tmpStr;
		p.name=TRANS("Num bins z");
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of voxels to use in Z direction");
		propertyList.addProperty(p,curGroup);
	}

	//Let the user know what the valid values for voxel value types are
	vector<pair<unsigned int,string> > choices;
	tmpStr=getNormaliseTypeString(VOXELISE_NORMALISETYPE_NONE);
	choices.push_back(make_pair((unsigned int)VOXELISE_NORMALISETYPE_NONE,tmpStr));
	tmpStr=getNormaliseTypeString(VOXELISE_NORMALISETYPE_VOLUME);
	choices.push_back(make_pair((unsigned int)VOXELISE_NORMALISETYPE_VOLUME,tmpStr));
	if(rsdIncoming)
	{
		//Concentration mode
		tmpStr=getNormaliseTypeString(VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL);
		choices.push_back(make_pair((unsigned int)VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL,tmpStr));
		//Ratio is only valid if we have a way of separation for the ions i.e. range
		tmpStr=getNormaliseTypeString(VOXELISE_NORMALISETYPE_COUNT2INVOXEL);
		choices.push_back(make_pair((unsigned int)VOXELISE_NORMALISETYPE_COUNT2INVOXEL,tmpStr));
	}

	tmpStr= choiceString(choices,normaliseType);
	p.name=TRANS("Normalise by");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Method to use to normalise scalar value in each voxel");
	p.key=KEY_NORMALISE_TYPE;
	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Computation"));

	curGroup++;
	
	// numerator
	if (rsdIncoming) 
	{
		p.name=TRANS("Numerator");
		p.data=numeratorAll ? "1" : "0";
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Parmeter \"a\" used in fraction (a/b) to get voxel value");
		p.key=KEY_ENABLE_NUMERATOR;
		propertyList.addProperty(p,curGroup);

		ASSERT(rsdIncoming->enabledIons.size()==enabledIons[0].size());	
		ASSERT(rsdIncoming->enabledIons.size()==enabledIons[1].size());	

		//Look at the numerator	
		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{
			string str;
			if(enabledIons[0][ui])
				str="1";
			else
				str="0";

			//Append the ion name with a checkbox
			p.name=rsdIncoming->rangeFile->getName(ui);
			p.data=str;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Enable this ion for numerator");
			p.key=KEY_ENABLE_NUMERATOR*1000+ui;
			propertyList.addProperty(p,curGroup);
		}
	
		curGroup++;
	}
	
	
	if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL && rsdIncoming) 
	{
		p.name=TRANS("Denominator");
		p.data=denominatorAll ? "1" : "0";
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Parameter \"b\" used in fraction (a/b) to get voxel value");
		p.key=KEY_ENABLE_DENOMINATOR;

		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{			
			string str;
			if(enabledIons[1][ui])
				str="1";
			else
				str="0";

			//Append the ion name with a checkbox
			p.key=KEY_ENABLE_DENOMINATOR*1000 + ui;
			p.data=str;
			p.name=rsdIncoming->rangeFile->getName(ui);
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Enable this ion for denominator contribution");

			propertyList.addProperty(p,curGroup);
		}
		curGroup++;
	}

	//Start a new set for filtering
	//----
	//TODO: Other filtering? threshold/median? laplacian? etc
	
	choices.clear();

	//Post-filtering method
	for(unsigned int ui=0;ui<VOXELISE_FILTERTYPE_MAX; ui++)
	{
		tmpStr=getFilterTypeString(ui);
		choices.push_back(make_pair(ui,tmpStr));
	}
	tmpStr= choiceString(choices,filterMode);

	p.name=TRANS("Filtering");
	p.data=tmpStr;
	p.key=KEY_FILTER_MODE;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Smoothing method to use on voxels");

	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Processing"));
	if(filterMode != VOXELISE_FILTERTYPE_NONE)
	{

		//Filter size
		stream_cast(tmpStr,filterBins);
		p.name=TRANS("Kernel Bins");
		p.data=tmpStr;
		p.key=KEY_FILTER_BINS;
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of bins in convolution kernel");
		propertyList.addProperty(p,curGroup);
		//Boundary wrapping mode selection
		choices.clear();
		for(unsigned int ui=0;ui<VOXELISE_FILTERBOUNDMODE_MAX; ui++)
		{
			tmpStr=getFilterBoundTypeString(ui);
			choices.push_back(make_pair(ui,tmpStr));
		}
		
		tmpStr= choiceString(choices,filterBoundaryMode);
		p.name=TRANS("Exterior values");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_CHOICE;
		p.helpText=TRANS("Method to use to treat boundaries of voxel data for convolution");
		p.key=KEY_FILTER_BOUNDARY_MODE;
		propertyList.addProperty(p,curGroup);
	}
	propertyList.setGroupTitle(curGroup,TRANS("Filtering"));
	curGroup++;
	//----

	//start a new group for the visual representation
	//----------------------------
	choices.clear();
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_POINTCLOUD);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_POINTCLOUD,tmpStr));
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_ISOSURF);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_ISOSURF,tmpStr));
	
	tmpStr= choiceString(choices,representation);

	p.name=TRANS("Representation");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("3D display method");
	p.key=KEY_VOXEL_REPRESENTATION_MODE;
	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Appearance"));

	switch(representation)
	{
		case VOXEL_REPRESENT_POINTCLOUD:
		{
			stream_cast(tmpStr,splatSize);
			p.name=TRANS("Spot size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Size of the spots to use for display");
			p.key=KEY_SPOTSIZE;
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,1.0-a);
			p.name=TRANS("Transparency");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("How \"see through\" each point is (0 - opaque, 1 - invisible)");
			p.key=KEY_TRANSPARANCY;
			propertyList.addProperty(p,curGroup);
			break;
		}
		case VOXEL_REPRESENT_ISOSURF:
		{
			stream_cast(tmpStr,isoLevel);
			p.name=TRANS("Isovalue");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Scalar value to show as isosurface");
			p.key=KEY_ISOLEVEL;
			propertyList.addProperty(p,curGroup);
		
				

			//Convert the ion colour to a hex string	
			genColString((unsigned char)(r*255),(unsigned char)(g*255),
					(unsigned char)(b*255),(unsigned char)(a*255),tmpStr);
			p.name=TRANS("Colour");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_COLOUR;
			p.helpText=TRANS("Colour of isosurface");
			p.key=KEY_COLOUR;
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,1.0-a);
			p.name=TRANS("Transparency");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("How \"see through\" each facet is (0 - opaque, 1 - invisible)");
			p.key=KEY_TRANSPARANCY;
			propertyList.addProperty(p,curGroup);
			
			break;
		}
		default:
			ASSERT(false);
			;
	}
	
	//----------------------------
}

bool VoxeliseFilter::setProperty(  unsigned int key,
									  const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{
		case KEY_FIXEDWIDTH: 
		{
			bool b;
			if(stream_cast(b,value))
				return false;

			//if the result is different, the
			//cache should be invalidated
			if(b!=fixedWidth)
			{
				needUpdate=true;
				fixedWidth=b;
				clearCache();
			}
			break;
		}	
		case KEY_NBINSX:
		{
			 unsigned int i;
			if(stream_cast(i,value))
				return false;
			if(!i)
				return false;

			needUpdate=true;
			nBins[0]=i;
			//if the result is different, the
			//cache should be invalidated
			if(i!=nBins[0])
			{
				needUpdate=true;
				nBins[0]=i;
				calculateWidthsFromNumBins(binWidth, nBins);
				clearCache();
			}
			break;
		}
		case KEY_NBINSY:
		{
			 unsigned int i;
			if(stream_cast(i,value))
				return false;
			if(!i)
				return false;
			needUpdate=true;
			//if the result is different, the
			//cache should be invalidated
			if(i!=nBins[1])
			{
				needUpdate=true;
				nBins[1]=i;
				calculateWidthsFromNumBins(binWidth, nBins);
				clearCache();
			}
			break;
		}
		case KEY_NBINSZ:
		{
			 unsigned int i;
			if(stream_cast(i,value))
				return false;
			if(!i)
				return false;
			
			//if the result is different, the
			//cache should be invalidated
			if(i!=nBins[2])
			{
				needUpdate=true;
				nBins[2]=i;
				calculateWidthsFromNumBins(binWidth, nBins);
				clearCache();
			}
			break;
		}
		case KEY_WIDTHBINSX:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
		
			if(f!=binWidth[0])
			{
				needUpdate=true;
				binWidth[0]=f;
				calculateNumBinsFromWidths(binWidth, nBins);
				clearCache();
			}
			break;
		}
		case KEY_WIDTHBINSY:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			
			if(f!=binWidth[1])
			{
				needUpdate=true;
				binWidth[1]=f;
				calculateNumBinsFromWidths(binWidth, nBins);
				clearCache();
			}
			break;
		}
		case KEY_WIDTHBINSZ:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			if(f!=binWidth[2])
			{
				needUpdate=true;
				binWidth[2]=f;
				calculateNumBinsFromWidths(binWidth, nBins);
				clearCache();
			}
			break;
		}
		case KEY_NORMALISE_TYPE:
		{
			unsigned int i;
			for(i = 0; i < VOXELISE_NORMALISETYPE_MAX; i++)
				if (value == getNormaliseTypeString(i)) break;
			if (i == VOXELISE_NORMALISETYPE_MAX)
				return false;
			if(normaliseType!=i)
			{
				needUpdate=true;
				clearCache();
				normaliseType=i;
			}
			break;
		}
		case KEY_SPOTSIZE:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			if(f !=splatSize)
			{
				splatSize=f;
				needUpdate=true;

				//Go in and manually adjust the cached
				//entries to have the new value, rather
				//than doing a full recomputation
				if(cacheOK)
				{
					for(unsigned int ui=0;ui<filterOutputs.size();ui++)
					{
						VoxelStreamData *d;
						d=(VoxelStreamData*)filterOutputs[ui];
						d->splatSize=splatSize;
					}
				}

			}
			break;
		}
		case KEY_TRANSPARANCY:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f < 0.0f || f > 1.0)
				return false;
			needUpdate=true;
			//Alpha is opacity, which is 1-transparancy
			a=1.0f-f;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{
					VoxelStreamData *d;
					d=(VoxelStreamData*)filterOutputs[ui];
					d->a=a;
				}
			}
			break;
		}
		case KEY_ISOLEVEL:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			isoLevel=f;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{
					VoxelStreamData *d;
					d=(VoxelStreamData*)filterOutputs[ui];
					d->isoLevel=isoLevel;
				}
			}
			break;
		}
		case KEY_COLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != b || newR != r ||
				newG !=g || newA != a)
				needUpdate=true;
			r=newR/255.0;
			g=newG/255.0;
			b=newB/255.0;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{
					VoxelStreamData *d;
					d=(VoxelStreamData*)filterOutputs[ui];
					d->r=r;
					d->g=g;
					d->b=b;
				}
			}
			break;
		}
		case KEY_VOXEL_REPRESENTATION_MODE:
		{
			 unsigned int i;
			for (i = 0; i < VOXEL_REPRESENT_END; i++)
				if (value == getRepresentTypeString(i)) break;
			if (i == VOXEL_REPRESENT_END)
				return false;
			needUpdate=true;
			representation=i;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{
					VoxelStreamData *d;
					d=(VoxelStreamData*)filterOutputs[ui];
					d->representationType=representation;
				}
			}
			break;
		}
		case KEY_ENABLE_NUMERATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[0].size(); i++) 
				enabledIons[0][i] = b;
			numeratorAll = b;
			needUpdate=true;
			clearCache();
			break;
		}
		case KEY_ENABLE_DENOMINATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
	
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[1].size(); i++) 
				enabledIons[1][i] = b;
			
			denominatorAll = b;
			needUpdate=true;			
			clearCache();
			break;
		}
		case KEY_FILTER_MODE:
		{
			 unsigned int i;
			for (i = 0; i < VOXEL_REPRESENT_END; i++)
				if (value == getFilterTypeString(i)) break;
			if (i == VOXEL_REPRESENT_END)
				return false;
			if(i!=filterMode)
			{
				needUpdate=true;
				filterMode=i;
				clearCache();
			}
			break;
		}
		case KEY_FILTER_BOUNDARY_MODE:
		{
			 unsigned int i;
			for (i = 0; i < VOXELISE_FILTERBOUNDMODE_MAX; i++)
				if (value == getFilterBoundTypeString(i)) break;
			if (i == VOXELISE_FILTERTYPE_MAX)
				return false;
			
			if(i != filterBoundaryMode)
			{
				filterBoundaryMode=i;
				needUpdate=true;
				clearCache();
			}
			break;
		}
		case KEY_FILTER_BINS:
		{
			 unsigned int i;
			if(stream_cast(i,value))
				return false;

			//FIXME: Min restriction is artificial and imposed due to incomplete separable convolution filter implementation
			if(i == 0 || i > std::min(nBins[0],std::min(nBins[1],nBins[2])))
				return false;
			if(i != filterBins)
			{
				needUpdate=true;
				filterBins=i;
				clearCache();
			}
			break;
		}
		default:
		{
			if (key >= KEY_ENABLE_DENOMINATOR*1000) {
				bool b;
				if(stream_cast(b,value))
					return false;

				enabledIons[1][key - KEY_ENABLE_DENOMINATOR*1000]=b;
				if (!b) {
					denominatorAll = false;
				}
				needUpdate=true;			
				clearCache();
			} else if (key >= KEY_ENABLE_NUMERATOR*1000) {
				bool b;
				if(stream_cast(b,value))
					return false;
				
				enabledIons[0][key - KEY_ENABLE_NUMERATOR*1000]=b;
				if (!b) {
					numeratorAll = false;
				}
				needUpdate=true;			
					clearCache();
			}
			else
			{
				ASSERT(false);
			}
			break;
		}
	}
	return true;
}

std::string  VoxeliseFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case VOXELISE_ABORT_ERR:
			return std::string(TRANS("Voxelisation aborted"));
		case VOXELISE_MEMORY_ERR:
			return std::string(TRANS("Out of memory"));
		case VOXELISE_CONVOLVE_ERR:
			return std::string(TRANS("Unable to perform filter convolution"));
		case VOXELISE_BOUNDS_INVALID_ERR:
			return std::string(TRANS("Voxelisation bounds are invalid"));
	}	
	
	return std::string("BUG! Should not see this (VoxeliseFilter)");
}

bool VoxeliseFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\"" << escapeXML(userString) << "\"/>" << endl;
			f << tabs(depth+1) << "<fixedwidth value=\""<<fixedWidth << "\"/>"  << endl;
			f << tabs(depth+1) << "<nbins values=\""<<nBins[0] << ","<<nBins[1]<<","<<nBins[2] << "\"/>"  << endl;
			f << tabs(depth+1) << "<binwidth values=\""<<binWidth[0] << ","<<binWidth[1]<<","<<binWidth[2] << "\"/>"  << endl;
			f << tabs(depth+1) << "<normalisetype value=\""<<normaliseType << "\"/>"  << endl;
			f << tabs(depth+1) << "<enabledions>" << endl;

			f << tabs(depth+2) << "<numerator>" << endl;
			for(unsigned int ui=0;ui<enabledIons[0].size(); ui++)
				f << tabs(depth+3) << "<enabled value=\"" << (enabledIons[0][ui]?1:0) << "\"/>" << endl;
			f << tabs(depth+2) << "</numerator>" << endl;

			f << tabs(depth+2) << "<denominator>" << endl;
			for(unsigned int ui=0;ui<enabledIons[1].size(); ui++)
				f << tabs(depth+3) << "<enabled value=\"" << (enabledIons[1][ui]?1:0) << "\"/>" << endl;
			f << tabs(depth+2) << "</denominator>" << endl;

			f << tabs(depth+1) << "</enabledions>" << endl;

			f << tabs(depth+1) << "<representation value=\""<<representation << "\"/>" << endl;
			f << tabs(depth+1) << "<isovalue value=\""<<isoLevel << "\"/>" << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;
			f << tabs(depth) << "</" << trueName() <<">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}
	
	return true;
}

bool VoxeliseFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;
	xmlChar *xmlString;
	stack<xmlNodePtr> nodeStack;

	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve fixedWidth mode
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"fixedwidth","value"))
		return false;
	if(tmpStr == "1") 
		fixedWidth=true;
	else if(tmpStr== "0")
		fixedWidth=false;
	else
		return false;
	
	//Retrieve nBins	
	if(XMLHelpFwdToElem(nodePtr,"nbins"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"values");
	if(!xmlString)
		return false;
	std::vector<string> v1;
	splitStrsRef((char *)xmlString,',',v1);
	for (size_t i = 0; i < INDEX_LENGTH && i < v1.size(); i++)
	{
		if(stream_cast(nBins[i],v1[i]))
			return false;
		
		if(nBins[i] <= 0)
			return false;
	}
	xmlFree(xmlString);
	
	//Retrieve bin width 
	if(XMLHelpFwdToElem(nodePtr,"binwidth"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"values");
	if(!xmlString)
		return false;
	std::vector<string> v2;
	splitStrsRef((char *)xmlString,',',v2);
	for (size_t i = 0; i < INDEX_LENGTH && i < v2.size(); i++)
	{
		if(stream_cast(binWidth[i],v2[i]))
			return false;
		
		if(binWidth[i] <= 0)
			return false;
	}
	xmlFree(xmlString);
	
	//Retrieve normaliseType
	if(!XMLGetNextElemAttrib(nodePtr,normaliseType,"normalisetype","value"))
		return false;
	if(normaliseType >= VOXELISE_NORMALISETYPE_MAX)
		return false;

	//Look for the enabled ions bit
	//-------	
	//
	
	if(!XMLHelpFwdToElem(nodePtr,"enabledions"))
	{

		nodeStack.push(nodePtr);
		if(!nodePtr->xmlChildrenNode)
			return false;
		nodePtr=nodePtr->xmlChildrenNode;
		
		//enabled ions for numerator
		if(XMLHelpFwdToElem(nodePtr,"numerator"))
			return false;

		nodeStack.push(nodePtr);

		if(!nodePtr->xmlChildrenNode)
			return false;

		nodePtr=nodePtr->xmlChildrenNode;

		while(nodePtr)
		{
			char c;
			//Retrieve representation
			if(!XMLGetNextElemAttrib(nodePtr,c,"enabled","value"))
				break;

			if(c == '1')
				enabledIons[0].push_back(true);
			else
				enabledIons[0].push_back(false);


			nodePtr=nodePtr->next;
		}

		nodePtr=nodeStack.top();
		nodeStack.pop();

		//enabled ions for denominator
		if(XMLHelpFwdToElem(nodePtr,"denominator"))
			return false;


		if(!nodePtr->xmlChildrenNode)
			return false;

		nodeStack.push(nodePtr);
		nodePtr=nodePtr->xmlChildrenNode;

		while(nodePtr)
		{
			char c;
			//Retrieve representation
			if(!XMLGetNextElemAttrib(nodePtr,c,"enabled","value"))
				break;

			if(c == '1')
				enabledIons[1].push_back(true);
			else
				enabledIons[1].push_back(false);
				

			nodePtr=nodePtr->next;
		}


		nodeStack.pop();
		nodePtr=nodeStack.top();
		nodeStack.pop();

		//Check that the enabled ions size makes at least some sense...
		if(enabledIons[0].size() != enabledIons[1].size())
			return false;

	}

	//-------	
	//Retrieve representation
	if(!XMLGetNextElemAttrib(nodePtr,representation,"representation","value"))
		return false;
	if(representation >=VOXEL_REPRESENT_END)
		return false;

	//-------	
	//Retrieve representation
	if(!XMLGetNextElemAttrib(nodePtr,isoLevel,"isovalue","value"))
		return false;

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;

	//====
	return true;
	
}

unsigned int VoxeliseFilter::getRefreshBlockMask() const
{
	//Ions, plots and voxels cannot pass through this filter
	return STREAM_TYPE_IONS | STREAM_TYPE_PLOT | STREAM_TYPE_VOXEL;
}

unsigned int VoxeliseFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_VOXEL | STREAM_TYPE_DRAW;
}

unsigned int VoxeliseFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
}

void VoxeliseFilter::setPropFromBinding(const SelectionBinding &b)
{
}	

#ifdef DEBUG
bool voxelSingleCountTest()
{
	//Test counting a single vector
	
	vector<IonHit> ionVec;

	ionVec.resize(5);
	ionVec[0].setPos(Point3D(0.1,0.1,0.1));
	ionVec[1].setPos(Point3D(0.1,0.0,0.1));
	ionVec[2].setPos(Point3D(0.0,0.1,0.1));
	ionVec[3].setPos(Point3D(0.1,0.1,0.0));
	ionVec[4].setPos(Point3D(0.0,0.1,0.0));

	for(unsigned int ui=0;ui<ionVec.size();ui++)
		ionVec[ui].setMassToCharge(1);

	IonStreamData *ionData = new IonStreamData;
	std::swap(ionData->data,ionVec);
	
	size_t numIons=ionData->data.size();
	
	VoxeliseFilter *f = new VoxeliseFilter;
	f->setCaching(false);

	bool needUpdate;
	TEST(f->setProperty(KEY_NBINSX,"4",needUpdate),"num bins x");
	TEST(f->setProperty(KEY_NBINSY,"4",needUpdate),"num bins y");
	TEST(f->setProperty(KEY_NBINSZ,"4",needUpdate),"num bins z");


	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(ionData);

	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"Refresh error code");
	delete f;

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_VOXEL,"Stream type");


	const VoxelStreamData *v= (const VoxelStreamData*)streamOut[0];

	TEST(v->data.max() <=numIons,
			"voxel max less than input stream")

	TEST(v->data.min() >= 0.0f,"voxel counting minimum sanity");

	
	float dataSum;
	sumVoxels(v->data,dataSum);
	TEST(fabs(dataSum - (float)numIons ) < 
		sqrt(std::numeric_limits<float>::epsilon()),"voxel counting all input ions ");

	delete ionData;
	delete streamOut[0];

	return true;
}

bool voxelMultiCountTest()
{
	//Test counting multiple data streams containing ranged data 
	
	vector<const FilterStreamData*> streamIn,streamOut;
	vector<IonHit> ionVec;

	ionVec.resize(5);
	ionVec[0].setPos(Point3D(0.1,0.1,0.1));
	ionVec[1].setPos(Point3D(0.1,0.0,0.1));
	ionVec[2].setPos(Point3D(0.0,0.1,0.1));
	ionVec[3].setPos(Point3D(0.1,0.1,0.0));
	ionVec[4].setPos(Point3D(0.0,0.1,0.0));

	IonStreamData *ionData[2];
	RangeStreamData *rngStream;
	rngStream = new RangeStreamData;
	rngStream->rangeFile= new RangeFile;

	RGBf col; col.red=col.green=col.blue=1.0f;

	const unsigned int MAX_NUM_RANGES=2;
	for(unsigned int ui=0;ui<MAX_NUM_RANGES;ui++)
	{
		size_t ionNum;

		//Add a new ion "a1, a2... etc"
		string sTmp,sTmp2;
		sTmp="a";
		stream_cast(sTmp2,ui);
		sTmp+=sTmp2;
		ionNum=rngStream->rangeFile->addIon(sTmp,sTmp,col);
		rngStream->rangeFile->addRange((float)ui-0.5f,(float)ui+0.5f,ionNum);

		//Change m/c value for ion
		for(unsigned int uj=0;uj<ionVec.size();uj++)
			ionVec[uj].setMassToCharge(ui);
		
		ionData[ui]= new IonStreamData;
		ionData[ui]->data.resize(ionVec.size());
		std::copy(ionVec.begin(),ionVec.end(),ionData[ui]->data.begin());
		streamIn.push_back(ionData[ui]);
	}

	rngStream->enabledIons.resize(rngStream->rangeFile->getNumIons());
	rngStream->enabledRanges.resize(rngStream->rangeFile->getNumRanges());

	streamIn.push_back(rngStream);

	VoxeliseFilter *f = new VoxeliseFilter;

	//Initialise range data
	f->initFilter(streamIn,streamOut);


	f->setCaching(false);
	
	bool needUpdate;
	TEST(f->setProperty(KEY_NBINSX,"4",needUpdate),"num bins x");
	TEST(f->setProperty(KEY_NBINSY,"4",needUpdate),"num bins y");
	TEST(f->setProperty(KEY_NBINSZ,"4",needUpdate),"num bins z");


	TEST(f->setProperty(KEY_NORMALISE_TYPE,
		TRANS(NORMALISE_TYPE_STRING[VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL]),needUpdate), 
				"Set normalise mode");

	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"Refresh error code");
	delete f;
	for(unsigned int ui=0;ui<MAX_NUM_RANGES;ui++)
		delete streamIn[ui];
	TEST(streamOut.size() == 2,"stream count");
	TEST(streamOut[1]->getStreamType() == STREAM_TYPE_VOXEL,"Stream type");
	
	const VoxelStreamData *v= (const VoxelStreamData*)streamOut[1];

	TEST(v->data.max() <=1.0f,
			"voxel max less than input stream")
	TEST(v->data.min() >= 0.0f,"voxel counting minimum sanity");


	for(unsigned int ui=0;ui<v->data.getSize();ui++)
	{
		float delta;
		delta=(v->data.getData(ui) - v->data.getData(0) );
		ASSERT( v->data.getData(ui) == 0 || delta < std::numeric_limits<float>::epsilon());
	}

	delete v;

	delete rngStream->rangeFile;
	delete rngStream;

	return true;
}


bool VoxeliseFilter::runUnitTests()
{

	if(!voxelSingleCountTest())
		return false;

	if(!voxelMultiCountTest())
		return false;


	return true;
}

#endif
