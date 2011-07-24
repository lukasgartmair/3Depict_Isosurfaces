
#include "voxelise.h"
#include "../xmlHelper.h"

#include "../translation.h"

enum
{
	KEY_VOXELISE_FIXEDWIDTH,
	KEY_VOXELISE_NBINSX,
	KEY_VOXELISE_NBINSY,
	KEY_VOXELISE_NBINSZ,
	KEY_VOXELISE_WIDTHBINSX,
	KEY_VOXELISE_WIDTHBINSY,
	KEY_VOXELISE_WIDTHBINSZ,
	KEY_VOXELISE_COUNT_TYPE,
	KEY_VOXELISE_NORMALISE_TYPE,
	KEY_VOXELISE_SPOTSIZE,
	KEY_VOXELISE_TRANSPARANCY,
	KEY_VOXELISE_COLOUR,
	KEY_VOXELISE_ISOLEVEL,
	KEY_VOXEL_REPRESENTATION_MODE,
	KEY_VOXELISE_FILTER_MODE,
	KEY_VOXELISE_FILTER_BOUNDARY_MODE,
	KEY_VOXELISE_FILTER_BINS,
	KEY_VOXELISE_ENABLE_NUMERATOR,
	KEY_VOXELISE_ENABLE_DENOMINATOR
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

//This is not a member of voxels.h, as the voxels do not have any concept of the IonHit
int countPoints(Voxels<float> &v, const std::vector<IonHit> &points, 
				bool noWrap,bool (*callback)(void))
{

	size_t x,y,z;
	size_t binCount[3];
	v.getSize(binCount[0],binCount[1],binCount[2]);

	unsigned int downSample=MAX_CALLBACK;
	for (size_t ui=0; ui<points.size(); ui++)
	{
		if(!downSample--)
		{
			if(!(*callback)())
				return 1;
			downSample=MAX_CALLBACK;
		}
		v.getIndex(x,y,z,points[ui].getPos());

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
	//otherwise we dont until we are presetned with the boundcube.
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

//TODO:
unsigned int VoxeliseFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
										  std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
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
		if (is->GetNumBasicObjects() < 2) continue;
	
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
		
			//If the user requests normalisation, compute the denominator datset
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

			if(!(*callback)())
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
				
				if(!(*callback)())
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
			if(vs->data.separableConvolve(kernel,res,false,modeMap[filterBoundaryMode]))
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

std::string VoxeliseFilter::getNormaliseTypeString(int type) const {
	switch (type) {
		case VOXELISE_NORMALISETYPE_NONE:
			return std::string(TRANS("None (Raw count)"));
		case VOXELISE_NORMALISETYPE_VOLUME:
			return std::string(TRANS("Volume (Density)"));
		case VOXELISE_NORMALISETYPE_COUNT2INVOXEL:
			return std::string(TRANS("Ratio (Num/Denom)"));
		case VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL:
			return std::string(TRANS("All Ions (conc)"));
		default:
			ASSERT(false);
	}
}

std::string VoxeliseFilter::getRepresentTypeString(int type) const {
	switch (type) {
		case VOXEL_REPRESENT_POINTCLOUD:
			return std::string(TRANS("Point Cloud"));
		case VOXEL_REPRESENT_ISOSURF:
			return std::string(TRANS("Isosurface"));
		default:
			ASSERT(false);
	}
}

std::string VoxeliseFilter::getFilterTypeString(int type) const 
{
	switch (type)
	{
		case VOXELISE_FILTERTYPE_NONE:
			return std::string(TRANS("None"));
		case VOXELISE_FILTERTYPE_GAUSS:
			return std::string(TRANS("Gaussian (2𝜎)"));
		default:
			ASSERT(false);
	}
}


std::string VoxeliseFilter::getFilterBoundTypeString(int type) const 
{
	switch (type)
	{
		case VOXELISE_FILTERBOUNDMODE_ZERO:
			return std::string(TRANS("Zero"));
		case VOXELISE_FILTERBOUNDMODE_BOUNCE:
			return std::string(TRANS("Bounce"));
		default:
			ASSERT(false);
	}
}

void VoxeliseFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();
	
	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	stream_cast(tmpStr, fixedWidth);
	s.push_back(std::make_pair(TRANS("Fixed width"), tmpStr));
	keys.push_back(KEY_VOXELISE_FIXEDWIDTH);
	type.push_back(PROPERTY_TYPE_BOOL);
	
	if(fixedWidth)
	{
		stream_cast(tmpStr,binWidth[0]);
		keys.push_back(KEY_VOXELISE_WIDTHBINSX);
		s.push_back(make_pair(TRANS("Bin width x"), tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,binWidth[1]);
		keys.push_back(KEY_VOXELISE_WIDTHBINSY);
		s.push_back(make_pair(TRANS("Bin width y"), tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,binWidth[2]);
		keys.push_back(KEY_VOXELISE_WIDTHBINSZ);
		s.push_back(make_pair(TRANS("Bin width z"), tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);
	}
	else
	{
		stream_cast(tmpStr,nBins[0]);
		keys.push_back(KEY_VOXELISE_NBINSX);
		s.push_back(make_pair(TRANS("Num bins x"), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,nBins[1]);
		keys.push_back(KEY_VOXELISE_NBINSY);
		s.push_back(make_pair(TRANS("Num bins y"), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,nBins[2]);
		keys.push_back(KEY_VOXELISE_NBINSZ);
		s.push_back(make_pair(TRANS("Num bins z"), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
	}

	//Let the user know what the valid values for voxel value types are
	string tmpChoice;	
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
	s.push_back(make_pair(string(TRANS("Normalise by")),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_VOXELISE_NORMALISE_TYPE);
	
	
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	s.clear();
	type.clear();
	keys.clear();

		
	// numerator
	if (rsdIncoming) {
		s.push_back(make_pair(TRANS("Numerator"), numeratorAll ? "1" : "0"));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_VOXELISE_ENABLE_NUMERATOR);

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
			s.push_back(make_pair(
				rsdIncoming->rangeFile->getName(ui), str));
			type.push_back(PROPERTY_TYPE_BOOL);
			keys.push_back(KEY_VOXELISE_ENABLE_NUMERATOR*1000+ui);
		}
		propertyList.types.push_back(type);
		propertyList.data.push_back(s);
		propertyList.keys.push_back(keys);
	}
	
	s.clear();
	type.clear();
	keys.clear();
	
	if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL && rsdIncoming) {
		// denominator
		s.push_back(make_pair(TRANS("Denominator"), denominatorAll ? "1" : "0"));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_VOXELISE_ENABLE_DENOMINATOR);

		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{			
			string str;
			if(enabledIons[1][ui])
				str="1";
			else
				str="0";

			//Append the ion name with a checkbox
			s.push_back(make_pair(
				rsdIncoming->rangeFile->getName(ui), str));

			type.push_back(PROPERTY_TYPE_BOOL);
			keys.push_back(KEY_VOXELISE_ENABLE_DENOMINATOR*1000+ui);
		}
		propertyList.types.push_back(type);
		propertyList.data.push_back(s);
		propertyList.keys.push_back(keys);

		s.clear();
		type.clear();
		keys.clear();
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
	s.push_back(make_pair(TRANS("Filtering"), tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_VOXELISE_FILTER_MODE);
	
	if(filterMode != VOXELISE_FILTERTYPE_NONE)
	{

		//Filter size
		stream_cast(tmpStr,filterBins);
		s.push_back(make_pair(TRANS("Kernel Bins"), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		keys.push_back(KEY_VOXELISE_FILTER_BINS);

		//Boundary wrapping mode selection
		choices.clear();
		for(unsigned int ui=0;ui<VOXELISE_FILTERBOUNDMODE_MAX; ui++)
		{
			tmpStr=getFilterBoundTypeString(ui);
			choices.push_back(make_pair(ui,tmpStr));
		}
		tmpStr= choiceString(choices,filterBoundaryMode);
	
		s.push_back(make_pair(TRANS("Exterior values"), tmpStr));
		type.push_back(PROPERTY_TYPE_CHOICE);
		keys.push_back(KEY_VOXELISE_FILTER_MODE);

	}


	propertyList.types.push_back(type);
	propertyList.data.push_back(s);
	propertyList.keys.push_back(keys);
	

	s.clear();
	type.clear();
	keys.clear();
	//----

	//start a new set for the visual representation
	//----------------------------
	choices.clear();
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_POINTCLOUD);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_POINTCLOUD,tmpStr));
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_ISOSURF);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_ISOSURF,tmpStr));
	
	
	tmpStr= choiceString(choices,representation);
	s.push_back(make_pair(string(TRANS("Representation")),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_VOXEL_REPRESENTATION_MODE);

	switch(representation)
	{
		case VOXEL_REPRESENT_POINTCLOUD:
		{
			stream_cast(tmpStr,splatSize);
			s.push_back(make_pair(TRANS("Spot size"),tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXELISE_SPOTSIZE);

			stream_cast(tmpStr,1.0-a);
			s.push_back(make_pair(TRANS("Transparency"),tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXELISE_TRANSPARANCY);
			break;
		}
		case VOXEL_REPRESENT_ISOSURF:
		{
			stream_cast(tmpStr,isoLevel);
			s.push_back(make_pair(TRANS("Isovalue"),tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXELISE_ISOLEVEL);
		
				

			//Convert the ion colour to a hex string	
			genColString((unsigned char)(r*255),(unsigned char)(g*255),
					(unsigned char)(b*255),(unsigned char)(a*255),tmpStr);
			s.push_back(make_pair(TRANS("Colour"),tmpStr));
			type.push_back(PROPERTY_TYPE_COLOUR);
			keys.push_back(KEY_VOXELISE_COLOUR);

			stream_cast(tmpStr,1.0-a);
			s.push_back(make_pair(TRANS("Transparency"),tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXELISE_TRANSPARANCY);
			
			break;
		}
		default:
			ASSERT(false);
			;
	}
	
	//----------------------------
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool VoxeliseFilter::setProperty( unsigned int set, unsigned int key,
									  const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{
		case KEY_VOXELISE_FIXEDWIDTH: 
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
		case KEY_VOXELISE_NBINSX:
		{
			 unsigned int i;
			if(stream_cast(i,value))
				return false;
			if(i <= 0)
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
		case KEY_VOXELISE_NBINSY:
		{
			 unsigned int i;
			if(stream_cast(i,value))
				return false;
			if(i <= 0)
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
		case KEY_VOXELISE_NBINSZ:
		{
			 unsigned int i;
			if(stream_cast(i,value))
				return false;
			if(i <= 0)
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
		case KEY_VOXELISE_WIDTHBINSX:
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
		case KEY_VOXELISE_WIDTHBINSY:
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
		case KEY_VOXELISE_WIDTHBINSZ:
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
		case KEY_VOXELISE_NORMALISE_TYPE:
		{
			 unsigned int i;
			for (i = 0; i < VOXELISE_NORMALISETYPE_MAX; i++)
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
		case KEY_VOXELISE_SPOTSIZE:
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
		case KEY_VOXELISE_TRANSPARANCY:
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
		case KEY_VOXELISE_ISOLEVEL:
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
		case KEY_VOXELISE_COLOUR:
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
		case KEY_VOXELISE_ENABLE_NUMERATOR:
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
		case KEY_VOXELISE_ENABLE_DENOMINATOR:
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
		case KEY_VOXELISE_FILTER_MODE:
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
		case KEY_VOXELISE_FILTER_BOUNDARY_MODE:
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
		case KEY_VOXELISE_FILTER_BINS:
		{
			 unsigned int i;
			if(stream_cast(i,value))
				return false;

			//FIXME: Min restriction is artificial and imposed due to incomplete separerable convolution filter implementation
			if(i <= 0 || i > std::min(nBins[0],std::min(nBins[1],nBins[2])))
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
			if (key >= KEY_VOXELISE_ENABLE_DENOMINATOR*1000) {
				bool b;
				if(stream_cast(b,value))
					return false;
//				if (b && !rsdIncoming->enabledIons[key - KEY_VOXELISE_ENABLE_DENOMINATOR*1000]) {
//					return false;
//				}
				enabledIons[1][key - KEY_VOXELISE_ENABLE_DENOMINATOR*1000]=b;
				if (!b) {
					denominatorAll = false;
				}
				needUpdate=true;			
				clearCache();
			} else if (key >= KEY_VOXELISE_ENABLE_NUMERATOR*1000) {
				bool b;
				if(stream_cast(b,value))
					return false;
//				if (b && !rsdIncoming->enabledIons[key - KEY_VOXELISE_ENABLE_NUMERATOR*1000]) {
//					return false;
//				}
				enabledIons[0][key - KEY_VOXELISE_ENABLE_NUMERATOR*1000]=b;
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

bool VoxeliseFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\"" << userString << "\"/>" << endl;
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

		//enabled ions for demerator
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


	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;

	//====
	return true;
	
}

int VoxeliseFilter::getRefreshBlockMask() const
{
	//Ions, plots and voxels cannot pass through this filter
	return STREAM_TYPE_IONS | STREAM_TYPE_PLOT | STREAM_TYPE_VOXEL;
}

int VoxeliseFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_VOXEL | STREAM_TYPE_DRAW;
}


void VoxeliseFilter::setPropFromBinding(const SelectionBinding &b)
{
}	

