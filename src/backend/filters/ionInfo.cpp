/*
 *	ionInfo.cpp -Filter to compute various properties of valued point cloud
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
#include "ionInfo.h"

#include "filterCommon.h"

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


bool getRectilinearBounds(const std::vector<const FilterStreamData *> &dataIn, BoundCube &bound,
					unsigned int *progress, unsigned int totalSize,bool (*callback)(bool))
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
				IonHit::getBoundCube(ions->data,c);

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
			if(!(*callback)(false))
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

IonInfoFilter::IonInfoFilter() : wantIonCounts(true), wantNormalise(false),
	range(0), wantVolume(false), volumeAlgorithm(VOLUME_MODE_RECTILINEAR),
	cubeSideLen(1.0f)
{
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
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
{

	//Count the number of ions input
	size_t numTotalPoints = numElements(dataIn,STREAM_TYPE_IONS);
	size_t numRanged=0;


	if(!numTotalPoints)
	{
		consoleOutput.push_back((TRANS("No ions")));
		return 0;
	}

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
				{
					if(totalRanged)
						stream_cast(str,((float)numIons[ui])/(float)totalRanged);
					else
						str=TRANS("n/a");
				}
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


void IonInfoFilter::getProperties(FilterPropGroup &propertyList) const
{
	string str;
	FilterProperty p;
	size_t curGroup=0;

	vector<pair<unsigned int,string> > choices;
	string tmpStr;

	stream_cast(str,wantIonCounts);
	p.key=IONINFO_KEY_TOTALS;
	if(range)
	{
		p.name=TRANS("Compositions");
		p.helpText=TRANS("Display compositional data for points in console");
	}
	else
	{
		p.name=TRANS("Counts");
		p.helpText=TRANS("Display count data for points in console");
	}
	p.data= str;
	p.type=PROPERTY_TYPE_BOOL;
	propertyList.addProperty(p,curGroup);


	if(wantIonCounts && range)
	{
		stream_cast(str,wantNormalise);
		p.name=TRANS("Normalise");
		p.data=str;
		p.key=IONINFO_KEY_NORMALISE;
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Normalise count data");

		propertyList.addProperty(p,curGroup);
	}

	curGroup++;

	stream_cast(str,wantVolume);
	p.key=IONINFO_KEY_VOLUME;
	p.name=TRANS("Volume");
	p.data= str;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Compute volume for point data");
	propertyList.addProperty(p,curGroup);

	if(wantVolume)
	{
		for(unsigned int ui=0;ui<VOLUME_MODE_END; ui++)
		{
			choices.push_back(make_pair((unsigned int)ui,
						TRANS(volumeModeString[ui])));
		}
		
		tmpStr= choiceString(choices,volumeAlgorithm);
		p.name=TRANS("Algorithm");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_CHOICE;
		p.helpText=TRANS("Select volume counting technique");
		p.key=IONINFO_KEY_VOLUME_ALGORITHM;
		propertyList.addProperty(p,curGroup);


		switch(volumeAlgorithm)
		{
			case VOLUME_MODE_RECTILINEAR:
			case VOLUME_MODE_CONVEX:
				break;
		}
	
	}
}


bool IonInfoFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{
	string stripped=stripWhite(value);
	switch(key)
	{
		case IONINFO_KEY_TOTALS:
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
		case IONINFO_KEY_NORMALISE:
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
		case IONINFO_KEY_VOLUME:
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
		case IONINFO_KEY_VOLUME_ALGORITHM:
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
	ASSERT(false);
}

void IonInfoFilter::setPropFromBinding(const SelectionBinding &b)
{
	ASSERT(false);
}

unsigned int IonInfoFilter::convexHullEstimateVol(const vector<const FilterStreamData*> &data, 
								float &volume,bool (*callback)(bool))
{
	volume=0;

	//TODO: replace with real progress
	unsigned int dummyProgress;
	unsigned int errCode;

	//Compute the convex hull, leaving the qhull data structure intact
	const bool NO_FREE_QHULL=false;
	vector<Point3D> hullPts;
	if((errCode=computeConvexHull(data,&dummyProgress,callback,
						hullPts,NO_FREE_QHULL)))
		return errCode;

	Point3D midPt(0,0,0);

	for(size_t ui=0;ui<hullPts.size();ui++)
		midPt+=hullPts[ui];
	
	midPt*=1.0f/(float)hullPts.size();

	//We don't need this result anymore
	hullPts.clear();

	//OK, so we computed the hull. Good work.
	//now compute the volume
	facetT *curFac = qh facet_list;
	while(curFac != qh facet_tail)
	{
		vertexT *vertex;
		unsigned int ui;
		Point3D ptArray[3];

		//Facet should be a simplical facet (i.e. triangle)
		ASSERT(curFac->simplicial);

		ui=0;
		vertex  = (vertexT *)curFac->vertices->e[ui].p;
		while(vertex)
		{	//copy the vertex info into the pt array
			(ptArray[ui])[0]  = vertex->point[0];
			(ptArray[ui])[1]  = vertex->point[1];
			(ptArray[ui])[2]  = vertex->point[2];

			//increment before updating vertex
			//to allow checking for NULL termination
			ui++;
			vertex  = (vertexT *)curFac->vertices->e[ui].p;

		}

		//note that this counter has been post incremented.
		ASSERT(ui ==3);
		volume+=pyramidVol(ptArray,midPt);


		curFac=curFac->next;
	}


	//Free the convex hull mem
	FREE_QHULL();
	
	return 0;
}

bool IonInfoFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) <<  "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

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

unsigned int IonInfoFilter::getRefreshUseMask() const
{
	return  STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
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
	unsigned int numAngles=(unsigned int)( 180.0f/angularStep);

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
	TEST(f->setProperty(IONINFO_KEY_VOLUME,"1",needUp),"Set prop");
	string s;
	stream_cast(s,(int)VOLUME_MODE_RECTILINEAR);

	//Can return false if algorithm already selected. Do not
	// test return
	f->setProperty(IONINFO_KEY_VOLUME_ALGORITHM, s,needUp);
	
	
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(d);
	
	ProgressData p;
	f->refresh(streamIn,streamOut,p,dummyCallback);

	//No ions come out of the info
	TEST(streamOut.empty(),"stream size test");

	vector<string> consoleStrings;
	f->getConsoleStrings(consoleStrings); 
	
	//weak test for the console string size
	TEST(consoleStrings.size(), "console strings existence test");


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
	f->setProperty(IONINFO_KEY_VOLUME_ALGORITHM, s,needUp);
	
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback), "refresh");
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
	TEST(f->setProperty(IONINFO_KEY_VOLUME,"1",needUp),"Set prop");

	//Can return false if the default algorithm is the same
	//  as the selected algorithm
	f->setProperty(IONINFO_KEY_VOLUME_ALGORITHM, 
		volumeModeString[VOLUME_MODE_RECTILINEAR],needUp);
	
	
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(d);
	
	ProgressData p;
	f->refresh(streamIn,streamOut,p,dummyCallback);

	//No ions come out of the info
	TEST(streamOut.empty(),"stream size test");

	vector<string> consoleStrings;
	f->getConsoleStrings(consoleStrings); 

	//weak test for the console string size
	TEST(consoleStrings.size(), "console strings existence test");


	float volMeasure,volReal;
	volMeasure=f->getLastVolume();
	//Bounding box for sphere is diameter^3.
	volReal =8.0f*OUTLINE_RADIUS*OUTLINE_RADIUS*OUTLINE_RADIUS;
	TEST(fabs(volMeasure -volReal) < 0.05*volReal,"volume test (rect est of sphere)");

	
	//Try again, but with convex hull
	TEST(f->setProperty(IONINFO_KEY_VOLUME_ALGORITHM,
		volumeModeString[VOLUME_MODE_CONVEX],needUp),"Set prop");
	
	vector<string> dummy;
	f->getConsoleStrings(dummy);

	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh error code");

	volMeasure=f->getLastVolume();

	//Convex volume of sphere
	volReal =4.0f/3.0f*M_PI*OUTLINE_RADIUS*OUTLINE_RADIUS*OUTLINE_RADIUS;
	TEST(fabs(volMeasure -volReal) < 0.05*volReal, "volume test, convex est. of sphere");
	
	TEST(consoleStrings.size(), "console strings existence test");

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

