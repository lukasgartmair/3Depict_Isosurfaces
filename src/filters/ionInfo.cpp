#include "ionInfo.h"

#include "../xmlHelper.h"
#include "../translation.h"
#include "../voxels.h"

//QHull library
#ifdef __POWERPC__
	#pragma push_macro("__POWERPC__")
	#define __POWERPC__ 1
#endif
extern "C"
{
	#include <qhull/qhull_a.h>
}
#ifdef __POWERPC__
	#pragma pop_macro("__POWERPC__")
#endif

//TODO: Work out where the payoff for this is
//grab size when doing convex hull calculations
const unsigned int HULL_GRAB_SIZE=4096;

enum
{
	KEY_TOTALS=1,
	KEY_NORMALISE,
	KEY_VOLUME,
	KEY_VOLUME_ALGORITHM,
};


enum
{
	VOLUME_MODE_RECTILINEAR=0,
	VOLUME_MODE_CONVEX,
	VOLUME_MODE_END
};

const char *volumeModeString[] = {
       	NTRANS("Rectilinear"),
	NTRANS("Convex hull")
	};
				
enum
{
	ERR_NO_MEM,
	ERR_USER_ABORT,
	ERR_BAD_QHULL
};

unsigned int doHull(unsigned int bufferSize, double *buffer, 
			vector<Point3D> &resHull, Point3D &midPoint)
{
	const int dim=3;
	//Now compute the new hull
	//Generate the convex hull
	//(result is stored in qh's globals :(  )
	//note that the input is "joggled" to 
	//ensure simplicial facet generation

	qh_new_qhull(	dim,
			bufferSize,
			buffer,
			false,
			(char *)"qhull QJ", //Joggle the output, such that only simplical facets are generated
			NULL,
			NULL);

	unsigned int numPoints=0;
	//count points
	//--	
	//OKay, whilst this may look like invalid syntax,
	//qh is actually a macro from qhull
	//that creates qh. or qh-> as needed
	vertexT *vertex = qh vertex_list;
	while(vertex != qh vertex_tail)
	{
		vertex = vertex->next;
		numPoints++;
	}
	//--	

	//store points in vector
	//--
	vertex= qh vertex_list;
	try
	{
		resHull.resize(numPoints);	
	}
	catch(std::bad_alloc)
	{
		free(buffer);
		return ERR_NO_MEM;
	}
	//--

	//Compute mean point
	//--
	int curPt=0;
	midPoint=Point3D(0,0,0);	
	while(vertex != qh vertex_tail)
	{
		resHull[curPt]=Point3D(vertex->point[0],
				vertex->point[1],
				vertex->point[2]);
		midPoint+=resHull[curPt];
		curPt++;
		vertex = vertex->next;
	}
	midPoint*=1.0f/(float)numPoints;
	//--

	return 0;
}

bool getRectilinearBounds(const std::vector<const FilterStreamData *> &dataIn, BoundCube &bound,
					unsigned int *progress, unsigned int totalSize,bool (*callback)(void))
{
	bound.setInvalid();

	vector<Point3D> overflow;

	size_t n=0;
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
		{

			const IonStreamData *ions;
			ions = ( const IonStreamData *)dataIn[ui];
			n+=ions->data.size();
			BoundCube c;
			if(ions->data.size() >1)
			{
				ions = (const IonStreamData*)dataIn[ui];
				c=getIonDataLimits(ions->data);

				if(c.isValid())
				{
					if(bound.isValid())
						bound.expand(c);
					else
						bound=c;
				}
			}
			else
			{
				//Do we have single ions in their own
				//data structure? if so, they don't have a bound
				//on their own, but may have one collectively.
				if(ions->data.size())
					overflow.push_back(ions->data[0].getPos());
			}
		
			*progress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
			if(!(*callback)())
				return false;
		}
	}


	//Handle any single ions
	if(overflow.size() > 1)
	{
		BoundCube c;
		c.setBounds(overflow);
		if(bound.isValid())
			bound.expand(c);
		else
			bound=c;
	}
	else if(bound.isValid() && overflow.size() == 1)
		bound.expand(overflow[0]);

	return true;
}

IonInfoFilter::IonInfoFilter()
{
	volumeAlgorithm=VOLUME_MODE_RECTILINEAR;
	wantIonCounts=true;
	wantVolume=false;
	wantNormalise=false;
	range=0;

	cubeSideLen=1.0;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up
}

void IonInfoFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
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
		if(range)
			delete range;
		range=0;
	}
	else
	{
		//If we didn't have an incoming rsd, then make one up!
		if(!range)
		{
			range= new RangeStreamData;
			*range=*c;
		}
		else
		{

			//OK, so we have a range incoming already (from last time)
			//-- the question is, is it the same one we had before ?
			//Do a pointer comparison (its a hack, yes, but it should work)
			if(range->rangeFile != c->rangeFile)
			{
				//hmm, it is different. well, trash the old incoming rng
				delete range;

				range = new RangeStreamData;
				*range=*c;
			}

		}

	}

}

//TODO: Refactor -- this is a clone of countPoints from voxelise.cpp
//This is not a member of voxels.h, as the voxels do not have any concept of the IonHit
int countPoints(Voxels<size_t> &v, const std::vector<IonHit> &points, 
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

Filter *IonInfoFilter::cloneUncached() const
{
	IonInfoFilter *p=new IonInfoFilter();

	p->wantIonCounts=wantIonCounts;
	p->wantVolume=wantVolume;
	p->wantNormalise=wantNormalise;
	p->cubeSideLen=cubeSideLen;
	p->volumeAlgorithm=volumeAlgorithm;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	p->wantVolume=wantVolume;
	p->volumeAlgorithm=volumeAlgorithm;
	return p;
}

unsigned int IonInfoFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{

	//Count the number of ions input
	size_t numTotalPoints = numElements(dataIn,STREAM_TYPE_IONS);
	size_t numRanged=0;

	//Compute ion counts/composition as needed
	if(wantIonCounts)
	{
		std::string str;
		//Count the number of ions
		if(range)
		{
			vector<size_t> numIons;
			ASSERT(range);

			const RangeFile *r=range->rangeFile;
			numIons.resize(r->getNumIons()+1,0);

			//count ions per-species. Add a bin on the end for unranged
			for(size_t ui=0;ui<dataIn.size();ui++)
			{
				if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
					continue;

				const IonStreamData  *i; 
				i = (const IonStreamData *)dataIn[ui];

				for(size_t uj=0;uj<i->data.size(); uj++)
				{
					unsigned int id;
					id = r->getIonID(i->data[uj].getMassToCharge());
					if(id != (unsigned int) -1)
						numIons[id]++;
					else
						numIons[numIons.size()-1]++;
				}
			}
			
			stream_cast(str,numTotalPoints);
			str=std::string(TRANS("--Counts--") );
			consoleOutput.push_back(str);
			
			size_t totalRanged;
			if(wantNormalise)
			{
				totalRanged=0;
				//sum all ions *except* the unranged.
				for(size_t ui=0;ui<numIons.size()-1;ui++)
					totalRanged+=numIons[ui];
				
				stream_cast(str,totalRanged);
				str=TRANS("Total Ranged\t")+str;
			}
			else
			{
				stream_cast(str,numTotalPoints);
				str=TRANS("Total (incl. unranged)\t")+str;
			}
			consoleOutput.push_back(str);
			consoleOutput.push_back("");

			//Print out the ion count table
			for(size_t ui=0;ui<numIons.size();ui++)
			{
				if(wantNormalise)
					stream_cast(str,((float)numIons[ui])/(float)totalRanged);
				else
					stream_cast(str,numIons[ui]);
			
				if(ui!=numIons.size()-1)
					str=std::string(r->getName(ui)) + std::string("\t") + str;
				else
				{
					//output unranged count 
					str=std::string(TRANS("Unranged")) + std::string("\t") + str;
				}

				consoleOutput.push_back(str);
			}
			str=std::string("----------");
			consoleOutput.push_back(str);
	
			
			for(size_t ui=0;ui<numIons.size();ui++)
				numRanged++;
		}
		else
		{
			//ok, no ranges -- just give us the total
			stream_cast(str,numTotalPoints);
			str=std::string(TRANS("Number of points : ") )+ str;
			consoleOutput.push_back(str);
		}
	}

	float computedVol;
	//Compute volume as needed
	if(wantVolume)
	{
		switch(volumeAlgorithm)
		{
			case VOLUME_MODE_RECTILINEAR:
			{
				BoundCube bound;
				if(!getRectilinearBounds(dataIn,bound,
					&(progress.filterProgress),numTotalPoints,callback))
					return ERR_USER_ABORT;

				if(bound.isValid())
				{
					Point3D low,hi;
					string tmpLow,tmpHi,s;
					bound.getBounds(low,hi);
					computedVol=bound.volume();
					
					
					stream_cast(tmpLow,low);
					stream_cast(tmpHi,hi);
					
					s=TRANS("Rectilinear Bounds : ");
					s+= tmpLow + " / "  + tmpHi;
					consoleOutput.push_back(s);
					
					stream_cast(s,computedVol);
					consoleOutput.push_back(string(TRANS("Volume (len^3): ")) + s);
				}

				break;
			}
			case VOLUME_MODE_CONVEX:
			{
				//OK, so here we need to do a convex hull estimation of the volume.
				unsigned int err;
				err=convexHullEstimateVol(dataIn,computedVol,callback);
				if(err)
					return err;

				std::string s;
				stream_cast(s,computedVol);
				if(computedVol>0)
					consoleOutput.push_back(string(TRANS("Convex Volume (len^3): ")) + s);
				else
					consoleOutput.push_back(string(TRANS("Unable to compute volume")));


				break;
			}
			default:
				ASSERT(false);

		}
	
#ifdef DEBUG
		lastVolume=computedVol;
#endif
	}


	//"Pairwise events" - where we perform an action if both 
	//These
	if(wantIonCounts && wantVolume)
	{
		if(computedVol > sqrt(std::numeric_limits<float>::epsilon()))
		{
			float density;
			std::string s;
		
			if(range)
			{
				density=(float)numRanged/computedVol;
				stream_cast(s,density);
				consoleOutput.push_back(string(TRANS("Ranged Density (pts/vol):")) + s );
			}
			
			density=(float)numTotalPoints/computedVol;
			stream_cast(s,density);
			consoleOutput.push_back(string(TRANS("Total Density (pts/vol):")) + s );

	
		}
	}

	return 0;
}

size_t IonInfoFilter::numBytesForCache(size_t nObjects) const
{
	return 0;
}


void IonInfoFilter::getProperties(FilterProperties &propertyList) const
{
	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;
	string str;

	vector<pair<unsigned int,string> > choices;
	string tmpChoice,tmpStr;

	stream_cast(str,wantIonCounts);
	keys.push_back(KEY_TOTALS);
	if(range)
		s.push_back(make_pair(TRANS("Compositions"), str));
	else
		s.push_back(make_pair(TRANS("Counts"), str));

	type.push_back(PROPERTY_TYPE_BOOL);

	if(wantIonCounts && range)
	{
		stream_cast(str,wantNormalise);
		s.push_back(make_pair(TRANS("Normalise"),str));
		keys.push_back(KEY_NORMALISE);
		type.push_back(PROPERTY_TYPE_BOOL);

		propertyList.data.push_back(s);
		propertyList.keys.push_back(keys);
		propertyList.types.push_back(type);
		s.clear(); keys.clear(); type.clear();
	} 
	
	if(wantVolume) 	
	{
		propertyList.data.push_back(s);
		propertyList.keys.push_back(keys);
		propertyList.types.push_back(type);

		s.clear();keys.clear();type.clear();
	}

	stream_cast(str,wantVolume);
	keys.push_back(KEY_VOLUME);
	s.push_back(make_pair(TRANS("Volume"), str));
	type.push_back(PROPERTY_TYPE_BOOL);

	if(wantVolume)
	{
		for(unsigned int ui=0;ui<VOLUME_MODE_END; ui++)
		{
			choices.push_back(make_pair((unsigned int)ui,
						TRANS(volumeModeString[ui])));
		}
		
		tmpStr= choiceString(choices,volumeAlgorithm);
		s.push_back(make_pair(string(TRANS("Algorithm")),tmpStr));
		type.push_back(PROPERTY_TYPE_CHOICE);
		keys.push_back(KEY_VOLUME_ALGORITHM);


		switch(volumeAlgorithm)
		{
			case VOLUME_MODE_RECTILINEAR:
			case VOLUME_MODE_CONVEX:
				break;
		}
	
	}

	propertyList.data.push_back(s);
	propertyList.keys.push_back(keys);
	propertyList.types.push_back(type);

}


bool IonInfoFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{
	string stripped=stripWhite(value);
	switch(key)
	{
		case KEY_TOTALS:
		{
			if(!(stripped == "1"|| stripped == "0"))
				return false;
	
			bool newVal;
			newVal= (value == "1");	

			if(newVal == wantIonCounts)
				return false;
			wantIonCounts=newVal;	
			needUpdate=true;
			break;
		}
		case KEY_NORMALISE:
		{
			if(!(stripped == "1"|| stripped == "0"))
				return false;
	
			bool newVal;
			newVal= (value == "1");	

			if(newVal == wantNormalise)
				return false;
			wantNormalise=newVal;	
			needUpdate=true;
			break;
		}
		case KEY_VOLUME:
		{
			if(!(stripped == "1"|| stripped == "0"))
				return false;
	
			bool newVal;
			newVal= (value == "1");	

			if(newVal == wantVolume)
				return false;
			wantVolume=newVal;	
			needUpdate=true;
			break;
		}
		case KEY_VOLUME_ALGORITHM:
		{
			unsigned int newAlg=VOLUME_MODE_END;

			for(unsigned int ui=0;ui<VOLUME_MODE_END; ui++)
			{
				if(volumeModeString[ui] == value)
				{
					newAlg=ui;
					break;
				}
			}

			if(newAlg==volumeAlgorithm || newAlg == VOLUME_MODE_END)
				return false;
			
			volumeAlgorithm=newAlg;	
			needUpdate=true;
			break;	
		}
		default:
			ASSERT(false);
	}

	return true;
}

std::string  IonInfoFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case ERR_NO_MEM:
			return string(TRANS("Insufficient memory for operation"));
		case ERR_USER_ABORT:
			return string(TRANS("Aborted"));
		case ERR_BAD_QHULL:
			return string(TRANS("Bug? Problem with qhull library, cannot run convex hull."));
	}
}

unsigned int IonInfoFilter::convexHullEstimateVol(const vector<const FilterStreamData*> &data, 
								float &volume,bool (*callback)())const
{
	//OK, so heres the go. partition the input data
	//into GRAB_SIZE lots before processing hull.

	volume=0;
	double *buffer=0;
	double *tmp;
	//Use malloc so we can re-alloc
	buffer =(double*) malloc(HULL_GRAB_SIZE*3*sizeof(double));

	if(!buffer)
		return ERR_NO_MEM;

	size_t bufferSize=0;

	vector<Point3D> curHull;

	//Do the convex hull in steps for two reasons
	// 1) qhull chokes on large data
	// 2) we need to run the callback every now and again, so we have to 
	//   work in batches.
	Point3D midPoint;
	float maxSqrDist=-1;
	bool doneHull=false;
	for(size_t ui=0;ui<data.size(); ui++)
	{
		if(data[ui]->getStreamType() != STREAM_TYPE_IONS)
			continue;

		const IonStreamData* ions=(const IonStreamData*)data[ui];

		for(size_t uj=0; uj<ions->data.size(); uj++)
		{
			if(curHull.size())
			{
				//Do contained-in-sphere check
				if(midPoint.sqrDist(ions->data[uj].getPos()) < maxSqrDist)
					continue;
			}
			//Copy point data into hull buffer
			buffer[3*bufferSize]=ions->data[uj].getPos()[0];
			buffer[3*bufferSize+1]=ions->data[uj].getPos()[1];
			buffer[3*bufferSize+2]=ions->data[uj].getPos()[2];
			bufferSize++;


			if(bufferSize == HULL_GRAB_SIZE)
			{
				bufferSize+=curHull.size();
				tmp=(double*)realloc(buffer,
						3*bufferSize*sizeof(double));
 				if(!tmp)
				{
					free(buffer);
					return ERR_NO_MEM;
				}

				buffer=tmp;
				//Copy in the old hull
				for(size_t uk=0;uk<curHull.size();uk++)
				{	
					buffer[3*(HULL_GRAB_SIZE+uk)]=curHull[uk][0];
					buffer[3*(HULL_GRAB_SIZE+uk)+1]=curHull[uk][1];
					buffer[3*(HULL_GRAB_SIZE+uk)+2]=curHull[uk][2];
				}

				unsigned int errCode=0;
				if(doneHull)
				{
					//Free the last convex hull mem	
					qh_freeqhull(!qh_ALL);
					int curlong, totlong;
					qh_memfreeshort(&curlong, &totlong);
				}
				errCode=doHull(bufferSize,buffer,curHull,midPoint);
				if(errCode)
					return errCode;

				doneHull=true;

				//Now compute the min sqr distance
				//to the vertex, so we can fast-reject
				maxSqrDist=std::numeric_limits<float>::max();
				for(size_t ui=0;ui<curHull.size();ui++)
					maxSqrDist=std::min(maxSqrDist,curHull[ui].sqrDist(midPoint));
				//reset buffer size
				bufferSize=0;
			}


			if(!(*callback)())
			{
				free(buffer);
				return ERR_USER_ABORT;
			}

		}



	}


	//Need at least 3 objects to construct a sufficiently large buffer
	if(bufferSize + curHull.size() > 3)
	{
		//Re-allocate the buffer to determine the last hull size
		tmp=(double*)realloc(buffer,
			3*(bufferSize+curHull.size())*sizeof(double));
		if(!tmp)
		{
			free(buffer);
			return ERR_NO_MEM;
		}
		buffer=tmp;

		#pragma omp parallel for 
		for(unsigned int ui=0;ui<curHull.size();ui++)
		{
			buffer[3*(bufferSize+ui)]=curHull[ui][0];
			buffer[3*(bufferSize+ui)+1]=curHull[ui][1];
			buffer[3*(bufferSize+ui)+2]=curHull[ui][2];
		}

		unsigned int errCode=doHull(bufferSize+curHull.size(),buffer,curHull,midPoint);
		if(errCode)
		{
			free(buffer);
			return errCode;
		}
	}
	else if(bufferSize)
	{
		//we don't have enough points to run the convex hull algorithm;
		free(buffer);
		return 0;
	}


	//maybe we didn't have any points to hull?
	if(!curHull.size())
	{	
		free(buffer);
		return 0;
	}



	//OK, so we computed the hull. Good work.
	//now compute the volume
	facetT *curFac = qh facet_list;
	while(curFac != qh facet_tail)
	{
		vertexT *vertex;
		Point3D pyramidCentroid;
		unsigned int ui;
		Point3D ptArray[3];

		pyramidCentroid = midPoint;
		
		//This assertion fails, some more processing is needed to be done to break
		//the facet into something simplical
		ASSERT(curFac->simplicial);
		
		ui=0;
		vertex  = (vertexT *)curFac->vertices->e[ui].p;
		while(vertex)
		{	//copy the vertex info into the pt array
			(ptArray[ui])[0]  = vertex->point[0];
			(ptArray[ui])[1]  = vertex->point[1];
			(ptArray[ui])[2]  = vertex->point[2];
		
			//aggregate pyramidal points	
			pyramidCentroid += ptArray[ui];
		
			//increment before updating vertex
			//to allow checking for NULL termination	
			ui++;
			vertex  = (vertexT *)curFac->vertices->e[ui].p;
		
		}
		
		//note that this counter has been post incremented. 
		ASSERT(ui ==3);
		volume+=pyramidVol(ptArray,midPoint);
		

		curFac=curFac->next;
	}
	

	//Free the convex hull mem	
	qh_freeqhull(!qh_ALL);
	int curlong, totlong;
	qh_memfreeshort(&curlong, &totlong);
	free(buffer);

	return 0;
}

bool IonInfoFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) <<  "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<wantioncounts value=\""<<wantIonCounts<< "\"/>"  << endl;
			f << tabs(depth+1) << "<wantnormalise value=\""<<wantNormalise<< "\"/>"  << endl;
			f << tabs(depth+1) << "<wantvolume value=\""<<wantVolume<< "\"/>"  << endl;
			f << tabs(depth+1) << "<volumealgorithm value=\""<<volumeAlgorithm<< "\"/>"  << endl;
			f << tabs(depth+1) << "<cubesidelen value=\""<<cubeSideLen<< "\"/>"  << endl;

			f << tabs(depth) << "</" <<trueName()<< ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonInfoFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;

	xmlChar *xmlString;
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);

	//--
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"wantioncounts","value"))
		return false;
	if(tmpStr == "1")
		wantIonCounts=true;
	else if(tmpStr == "0")
		wantIonCounts=false;
	else
		return false;
	//--=
	
	//--
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"wantnormalise","value"))
		return false;
	if(tmpStr == "1")
		wantNormalise=true;
	else if(tmpStr == "0")
		wantNormalise=false;
	else
		return false;
	//--=


	//--
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"wantvolume","value"))
		return false;
	if(tmpStr == "1")
		wantVolume=true;
	else if(tmpStr == "0")
		wantVolume=false;
	else
		return false;
	//--=

	//--
	unsigned int tmpInt;
	if(!XMLGetNextElemAttrib(nodePtr,tmpInt,"volumealgorithm","value"))
		return false;

	if(tmpInt >=VOLUME_MODE_END)
		return false;
	volumeAlgorithm=tmpInt;
	//--=

	//--
	float tmpFloat;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"cubesidelen","value"))
		return false;

	if(tmpFloat <= 0.0f)
		return false;
	cubeSideLen=tmpFloat;
	//--=


	return true;
}

unsigned int IonInfoFilter::getRefreshBlockMask() const
{
	return STREAMTYPE_MASK_ALL;
}

unsigned int IonInfoFilter::getRefreshEmitMask() const
{
	return  0;
}


#ifdef DEBUG

void makeBox(float boxSize,IonStreamData *d)
{
	d->data.clear();
	for(unsigned int ui=0;ui<8;ui++)
	{
		IonHit h;
		float x,y,z;

		x= (float)(ui &1)*boxSize;
		y= (float)((ui &2) >> 1)*boxSize;
		z= (float)((ui &4) >> 2)*boxSize;

		h.setPos(Point3D(x,y,z));
		h.setMassToCharge(1);
		d->data.push_back(h);
	}
}
void makeSphereOutline(float radius, float angularStep,
			IonStreamData *d)
{
	d->clear();
	ASSERT(angularStep > 0.0f);
	unsigned int numAngles= 180.0/angularStep;

	for( unsigned int  ui=0; ui<numAngles; ui++)
	{
		float longit;
		//-0.5->0.5
		longit = (float)((int)ui-(int)(numAngles/2))/(float)(numAngles);
		//longitude test
		longit*=180.0f;

		for( unsigned int  uj=0; uj<numAngles; uj++)
		{
			float latit;
			//0->1
			latit = (float)((int)uj)/(float)(numAngles);
			latit*=180.0f;

			float x,y,z;
			x=radius*cos(longit)*sin(latit);
			y=radius*sin(longit)*sin(latit);
			z=radius*cos(latit);

			IonHit h;
			h.setPos(Point3D(x,y,z));
			h.setMassToCharge(1);
			d->data.push_back(h);
		}
	}
}

bool volumeBoxTest()
{
	//Construct a few boxes, then test each of their volumes
	IonStreamData *d=new IonStreamData();

	const float SOMEBOX=7.0f;
	makeBox(7.0,d);


	//Construct the filter, and then set up the options we need
	IonInfoFilter *f  = new IonInfoFilter;	
	f->setCaching(false);

	//activate volume measurement
	bool needUp;
	f->setProperty(0,KEY_VOLUME,"1",needUp);
	string s;
	stream_cast(s,(int)VOLUME_MODE_RECTILINEAR);
	f->setProperty(0,KEY_VOLUME_ALGORITHM, s,needUp);
	
	
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(d);
	
	ProgressData p;
	f->refresh(streamIn,streamOut,p,dummyCallback);

	//No ions come out of the info
	TEST(streamOut.size() == 0,"stream size test");

	vector<string> consoleStrings;
	f->getConsoleStrings(consoleStrings); 
	
	//weak test for the console string size
	TEST(consoleStrings.size(), "console strings existance test");


	//Ensure that the rectilinear volume is the same as
	// the theoretical value
	float volMeasure,volReal;;
	volMeasure=f->getLastVolume();
	volReal =SOMEBOX*SOMEBOX*SOMEBOX; 

	TEST(fabs(volMeasure -volReal) < 
		10.0f*sqrt(std::numeric_limits<float>::epsilon()),
					"volume estimation test (rect)");

	
	//Try again, but with convex hull
	stream_cast(s,(int)VOLUME_MODE_CONVEX);
	f->setProperty(0,KEY_VOLUME_ALGORITHM, s,needUp);
	f->refresh(streamIn,streamOut,p,dummyCallback);
	volMeasure=f->getLastVolume();

	TEST(fabs(volMeasure -volReal) < 
		10.0f*sqrt(std::numeric_limits<float>::epsilon()),
				"volume estimation test (convex)");
	



	delete d;
	delete f;
	return true;
}

bool volumeSphereTest()
{
	//Construct a few boxes, then test each of their volumes
	IonStreamData *d=new IonStreamData();

	const float OUTLINE_RADIUS=7.0f;
	const float ANGULAR_STEP=2.0f;
	makeSphereOutline(OUTLINE_RADIUS,ANGULAR_STEP,d);

	//Construct the filter, and then set up the options we need
	IonInfoFilter *f  = new IonInfoFilter;	
	f->setCaching(false);

	//activate volume measurement
	bool needUp;
	f->setProperty(0,KEY_VOLUME,"1",needUp);
	
	f->setProperty(0,KEY_VOLUME_ALGORITHM, 
		volumeModeString[VOLUME_MODE_RECTILINEAR],needUp);
	
	
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(d);
	
	ProgressData p;
	f->refresh(streamIn,streamOut,p,dummyCallback);

	//No ions come out of the info
	TEST(streamOut.size() == 0,"stream size test");

	vector<string> consoleStrings;
	f->getConsoleStrings(consoleStrings); 

	//weak test for the console string size
	TEST(consoleStrings.size(), "console strings existance test");


	float volMeasure,volReal;
	volMeasure=f->getLastVolume();
	//Bounding box for sphere is diameter^3.
	volReal =8.0f*OUTLINE_RADIUS*OUTLINE_RADIUS*OUTLINE_RADIUS;
	TEST(fabs(volMeasure -volReal) < 0.05*volReal,"volume test (rect est of sphere)");

	
	//Try again, but with convex hull
	f->setProperty(0,KEY_VOLUME_ALGORITHM,
		volumeModeString[VOLUME_MODE_CONVEX],needUp);
	
	vector<string> dummy;
	f->getConsoleStrings(dummy);

	f->refresh(streamIn,streamOut,p,dummyCallback);
	volMeasure=f->getLastVolume();

	//Convex volume of sphere
	volReal =4.0f/3.0f*M_PI*OUTLINE_RADIUS*OUTLINE_RADIUS*OUTLINE_RADIUS;
	TEST(fabs(volMeasure -volReal) < 0.05*volReal, "volume test, convex est. of sphere");
	
	TEST(consoleStrings.size(), "console strings existance test");

	delete d;
	delete f;
	return true;
}

bool IonInfoFilter::runUnitTests()
{
	if(!volumeBoxTest())
		return false;
	
	if(!volumeSphereTest())
		return false;
	
	return true;
}
#endif

