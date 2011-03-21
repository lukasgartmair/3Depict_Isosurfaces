
#include "voxelise.h"
#include "../xmlHelper.h"

enum
{
	KEY_VOXEL_FIXEDWIDTH,
	KEY_VOXEL_NBINSX,
	KEY_VOXEL_NBINSY,
	KEY_VOXEL_NBINSZ,
	KEY_VOXEL_WIDTHBINSX,
	KEY_VOXEL_WIDTHBINSY,
	KEY_VOXEL_WIDTHBINSZ,
	KEY_VOXEL_COUNT_TYPE,
	KEY_VOXEL_NORMALISE_TYPE,
	KEY_VOXEL_SPOTSIZE,
	KEY_VOXEL_TRANSPARANCY,
	KEY_VOXEL_COLOUR,
	KEY_VOXEL_ISOLEVEL,
	KEY_VOXEL_REPRESENTATION_MODE,
	KEY_VOXEL_ENABLE_NUMERATOR,
	KEY_VOXEL_ENABLE_DENOMINATOR,
};

//!Normalisation method
enum
{
	VOXEL_NORMALISETYPE_NONE,// straight count
	VOXEL_NORMALISETYPE_VOLUME,// density
	VOXEL_NORMALISETYPE_ALLATOMSINVOXEL, // concentration
	VOXEL_NORMALISETYPE_COUNT2INVOXEL,// ratio count1/count2
	VOXEL_NORMALISETYPE_MAX, // keep this at the end so it's a bookend for the last value
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
		if (x < binCount[0] && y < binCount[1] && z< binCount[2]
		        && x >= 0 && y >= 0 && z >= 0)
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
: fixedWidth(false), bc(), normaliseType(VOXEL_NORMALISETYPE_NONE)
{
	splatSize=1.0f;
	a=0.9f;
	r=g=b=0.5;
	isoLevel=0.5;
	bc.setBounds(0, 0, 0, 1, 1, 1);
	representation=VOXEL_REPRESENT_POINTCLOUD;
	for (unsigned int i = 0; i < INDEX_LENGTH; i++) {
		nBins[i] = 50;
	}
	calculateWidthsFromNumBins(binWidth, nBins);
	numeratorAll = false;
	denominatorAll = true;

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
	p->representation=representation;
	p->splatSize=splatSize;

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

// TODO: create plotstream
unsigned int VoxeliseFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
										  std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{	

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
	if (!bc.isValid() || bc.isFlat()) return VOXEL_BOUNDS_INVALID_ERR;

	bc.getBounds(minP,maxP);	
	if (fixedWidth) 
		calculateNumBinsFromWidths(binWidth, nBins);
	else
		calculateWidthsFromNumBins(binWidth, nBins);
	
	//Disallow empty bounding boxes (ie, produce no output)
	if(minP == maxP)
		return 0;
		
	VoxelStreamData *vs = new VoxelStreamData();
	vs->cached = 0;
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

	VoxelStreamData *vsDenom = NULL;
	if (normaliseType == VOXEL_NORMALISETYPE_COUNT2INVOXEL ||
		normaliseType == VOXEL_NORMALISETYPE_ALLATOMSINVOXEL) {
		//Check we actually have incoming data
		ASSERT(rsdIncoming);
		vsDenom = new VoxelStreamData();
		vsDenom->cached = 0;
		vsDenom->data.setCallbackMethod(callback);
		vsDenom->data.init(nBins[0], nBins[1], nBins[2], bc);
		vsDenom->representationType= representation;
		vsDenom->splatSize = splatSize;
		vsDenom->isoLevel=isoLevel;
		vsDenom->data.fill(0);
		vsDenom->a=a;
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
			if (normaliseType == VOXEL_NORMALISETYPE_COUNT2INVOXEL) {
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
						countPoints(vsDenom->data,is->data,true,callback);
				}
			} else if (normaliseType == VOXEL_NORMALISETYPE_ALLATOMSINVOXEL)
			{
				countPoints(vsDenom->data,is->data,true,callback);
			}

			if(!(*callback)())
			{
				delete vs;
				return VOXEL_ABORT_ERR;
			}
		}
	
		//Perform normalsiation	
		if (normaliseType == VOXEL_NORMALISETYPE_VOLUME)
			vs->data.calculateDensity();
		else if (normaliseType == VOXEL_NORMALISETYPE_COUNT2INVOXEL ||
				 normaliseType == VOXEL_NORMALISETYPE_ALLATOMSINVOXEL)
			vs->data /= vsDenom->data;
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
					return VOXEL_ABORT_ERR;
				}

			}
		}
		ASSERT(normaliseType != VOXEL_NORMALISETYPE_COUNT2INVOXEL
				&& normaliseType!=VOXEL_NORMALISETYPE_ALLATOMSINVOXEL);
		if (normaliseType == VOXEL_NORMALISETYPE_VOLUME)
			vs->data.calculateDensity();
	}	
	delete vsDenom;

	
	
	float min,max;
	vs->data.minMax(min,max);


	string sMin,sMax;
	stream_cast(sMin,min);
	stream_cast(sMax,max);
	consoleOutput.push_back(std::string("Voxel Limits (min,max): (") + sMin + string(",")
		       	+  sMax + ")");
	getOut.push_back(vs);

	return 0;
}

std::string VoxeliseFilter::getNormaliseTypeString(int type) const {
	switch (type) {
		case VOXEL_NORMALISETYPE_NONE:
			return std::string("None (Raw count)");
			break;
		case VOXEL_NORMALISETYPE_VOLUME:
			return std::string("Volume (Density)");
			break;
		case VOXEL_NORMALISETYPE_COUNT2INVOXEL:
			return std::string("Ratio (Num/Denom)");
			break;
		case VOXEL_NORMALISETYPE_ALLATOMSINVOXEL:
			return std::string("All Ions (conc)");
			break;
		default:
			return "";
			break;
	}
}

std::string VoxeliseFilter::getRepresentTypeString(int type) const {
	switch (type) {
		case VOXEL_REPRESENT_POINTCLOUD:
			return std::string("Point Cloud");
			break;
		case VOXEL_REPRESENT_ISOSURF:
			return std::string("Isosurface");
			break;
		default:
			return "";
			break;
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
	s.push_back(std::make_pair("Fixed width", tmpStr));
	keys.push_back(KEY_VOXEL_FIXEDWIDTH);
	type.push_back(PROPERTY_TYPE_BOOL);
	
	if(fixedWidth)
	{
		stream_cast(tmpStr,binWidth[0]);
		keys.push_back(KEY_VOXEL_WIDTHBINSX);
		s.push_back(make_pair("Bin width x", tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,binWidth[1]);
		keys.push_back(KEY_VOXEL_WIDTHBINSY);
		s.push_back(make_pair("Bin width y", tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,binWidth[2]);
		keys.push_back(KEY_VOXEL_WIDTHBINSZ);
		s.push_back(make_pair("Bin width z", tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);
	}
	else
	{
		stream_cast(tmpStr,nBins[0]);
		keys.push_back(KEY_VOXEL_NBINSX);
		s.push_back(make_pair("Num bins x", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,nBins[1]);
		keys.push_back(KEY_VOXEL_NBINSY);
		s.push_back(make_pair("Num bins y", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,nBins[2]);
		keys.push_back(KEY_VOXEL_NBINSZ);
		s.push_back(make_pair("Num bins z", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
	}

	//Let the user know what the valid values for voxel value types are
	string tmpChoice;	
	vector<pair<unsigned int,string> > choices;
	tmpStr=getNormaliseTypeString(VOXEL_NORMALISETYPE_NONE);
	choices.push_back(make_pair((unsigned int)VOXEL_NORMALISETYPE_NONE,tmpStr));
	tmpStr=getNormaliseTypeString(VOXEL_NORMALISETYPE_VOLUME);
	choices.push_back(make_pair((unsigned int)VOXEL_NORMALISETYPE_VOLUME,tmpStr));
	if(rsdIncoming)
	{
		//Concentration mode
		tmpStr=getNormaliseTypeString(VOXEL_NORMALISETYPE_ALLATOMSINVOXEL);
		choices.push_back(make_pair((unsigned int)VOXEL_NORMALISETYPE_ALLATOMSINVOXEL,tmpStr));
		//Ratio is only valid if we have a way of seperation for the ions i.e. range
		tmpStr=getNormaliseTypeString(VOXEL_NORMALISETYPE_COUNT2INVOXEL);
		choices.push_back(make_pair((unsigned int)VOXEL_NORMALISETYPE_COUNT2INVOXEL,tmpStr));
	}
	tmpStr= choiceString(choices,normaliseType);
	s.push_back(make_pair(string("Normalise by"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_VOXEL_NORMALISE_TYPE);
	
	
	//TODO
	//1. range file
	//2. threshold
	//3. gaussian
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	s.clear();
	type.clear();
	keys.clear();

		
	// numerator
	if (rsdIncoming) {
		s.push_back(make_pair("Numerator", numeratorAll ? "1" : "0"));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_VOXEL_ENABLE_NUMERATOR);

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
			keys.push_back(KEY_VOXEL_ENABLE_NUMERATOR*1000+ui);
		}
		propertyList.types.push_back(type);
		propertyList.data.push_back(s);
		propertyList.keys.push_back(keys);
	}
	
	s.clear();
	type.clear();
	keys.clear();
	
	if (normaliseType == VOXEL_NORMALISETYPE_COUNT2INVOXEL && rsdIncoming) {
		// denominator
		s.push_back(make_pair("Denominator", denominatorAll ? "1" : "0"));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_VOXEL_ENABLE_DENOMINATOR);

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
			keys.push_back(KEY_VOXEL_ENABLE_DENOMINATOR*1000+ui);
		}
		propertyList.types.push_back(type);
		propertyList.data.push_back(s);
		propertyList.keys.push_back(keys);

		s.clear();
		type.clear();
		keys.clear();
	}
	
	//start a new set for the visual representation
	//----------------------------
	choices.clear();
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_POINTCLOUD);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_POINTCLOUD,tmpStr));
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_ISOSURF);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_ISOSURF,tmpStr));
	
	
	tmpStr= choiceString(choices,representation);
	s.push_back(make_pair(string("Representation"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_VOXEL_REPRESENTATION_MODE);
	switch(representation)
	{
		case VOXEL_REPRESENT_POINTCLOUD:
		{
			stream_cast(tmpStr,splatSize);
			s.push_back(make_pair("Spot size",tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXEL_SPOTSIZE);

			stream_cast(tmpStr,1.0-a);
			s.push_back(make_pair("Transparency",tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXEL_TRANSPARANCY);
			break;
		}
		case VOXEL_REPRESENT_ISOSURF:
		{
			stream_cast(tmpStr,isoLevel);
			s.push_back(make_pair("Isovalue",tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXEL_ISOLEVEL);
		
				

			//Convert the ion colour to a hex string	
			genColString((unsigned char)(r*255),(unsigned char)(g*255),
					(unsigned char)(b*255),(unsigned char)(a*255),tmpStr);
			s.push_back(make_pair("Colour",tmpStr));
			type.push_back(PROPERTY_TYPE_COLOUR);
			keys.push_back(KEY_VOXEL_COLOUR);
			
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
		case KEY_VOXEL_FIXEDWIDTH: 
		{
			bool b;
			if(stream_cast(b,value))
				return false;
			fixedWidth=b;
			needUpdate=true;			
			break;
		}	
		case KEY_VOXEL_NBINSX:
		{
			int i;
			if(stream_cast(i,value))
				return false;
			if(i <= 0)
				return false;
			needUpdate=true;
			nBins[0]=i;
			calculateWidthsFromNumBins(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_NBINSY:
		{
			int i;
			if(stream_cast(i,value))
				return false;
			if(i <= 0)
				return false;
			needUpdate=true;
			nBins[1]=i;
			calculateWidthsFromNumBins(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_NBINSZ:
		{
			int i;
			if(stream_cast(i,value))
				return false;
			if(i <= 0)
				return false;
			needUpdate=true;
			nBins[2]=i;
			calculateWidthsFromNumBins(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_WIDTHBINSX:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			binWidth[0]=f;
			calculateNumBinsFromWidths(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_WIDTHBINSY:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			binWidth[1]=f;
			calculateNumBinsFromWidths(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_WIDTHBINSZ:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			binWidth[2]=f;
			calculateNumBinsFromWidths(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_NORMALISE_TYPE:
		{
			int i;
			for (i = 0; i < VOXEL_NORMALISETYPE_MAX; i++)
				if (value == getNormaliseTypeString(i)) break;
			if (i == VOXEL_NORMALISETYPE_MAX)
				return false;
			if(normaliseType!=i)
				needUpdate=true;
			normaliseType=i;
			break;
		}
		case KEY_VOXEL_SPOTSIZE:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			splatSize=f;
			break;
		}
		case KEY_VOXEL_TRANSPARANCY:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f || f > 1.0)
				return false;
			needUpdate=true;
			//Alpha is opacity, which is 1-transparancy
			a=1.0f-f;
			break;
		}
		case KEY_VOXEL_ISOLEVEL:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			isoLevel=f;
			break;
		}
		case KEY_VOXEL_COLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != b || newR != r ||
				newG !=g || newA != a)
				needUpdate=true;
			r=newR/255.0;
			g=newG/255.0;
			b=newB/255.0;
			break;
		}
		case KEY_VOXEL_REPRESENTATION_MODE:
		{
			int i;
			for (i = 0; i < VOXEL_REPRESENT_END; i++)
				if (value == getRepresentTypeString(i)) break;
			if (i == VOXEL_REPRESENT_END)
				return false;
			needUpdate=true;
			representation=i;
			break;
		}
		case KEY_VOXEL_ENABLE_NUMERATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[0].size(); i++) 
				enabledIons[0][i] = b;
			numeratorAll = b;
			needUpdate=true;			
			break;
		}
		case KEY_VOXEL_ENABLE_DENOMINATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
	
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[1].size(); i++) 
				enabledIons[1][i] = b;
			
			denominatorAll = b;
			needUpdate=true;			
			break;
		}
		default:
		{
			if (key >= KEY_VOXEL_ENABLE_DENOMINATOR*1000) {
				bool b;
				if(stream_cast(b,value))
					return false;
//				if (b && !rsdIncoming->enabledIons[key - KEY_VOXEL_ENABLE_DENOMINATOR*1000]) {
//					return false;
//				}
				enabledIons[1][key - KEY_VOXEL_ENABLE_DENOMINATOR*1000]=b;
				if (!b) {
					denominatorAll = false;
				}
				needUpdate=true;			
			} else if (key >= KEY_VOXEL_ENABLE_NUMERATOR*1000) {
				bool b;
				if(stream_cast(b,value))
					return false;
//				if (b && !rsdIncoming->enabledIons[key - KEY_VOXEL_ENABLE_NUMERATOR*1000]) {
//					return false;
//				}
				enabledIons[0][key - KEY_VOXEL_ENABLE_NUMERATOR*1000]=b;
				if (!b) {
					numeratorAll = false;
				}
				needUpdate=true;			
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
		case VOXEL_ABORT_ERR:
		{
			return std::string("Voxelisation aborted");
		}
		case VOXEL_BOUNDS_INVALID_ERR:
		{
			return std::string("Voxelisation bounds are invalid");
		}
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
	if(normaliseType >= VOXEL_NORMALISETYPE_MAX)
		return false;
	
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

void VoxeliseFilter::setPropFromBinding(const SelectionBinding &b)
{
}	

