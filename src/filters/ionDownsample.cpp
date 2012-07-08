#include "../APTClasses.h"
#include "../xmlHelper.h"

#include "../translation.h"

#include "ionDownsample.h"


//!Downsampling filter
enum
{
	IONDOWNSAMPLE_ABORT_ERR=1,
	IONDOWNSAMPLE_BAD_ALLOC,
};


// == Ion Downsampling filter ==

IonDownsampleFilter::IonDownsampleFilter()
{
	rng.initTimer();
	fixedNumOut=true;
	fraction=0.1f;
	maxAfterFilter=5000;
	rsdIncoming=0;
	perSpecies=false;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

void IonDownsampleFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	const RangeStreamData *c=0;
	//Determine if we have an incoming range
	for (size_t i = 0; i < dataIn.size(); i++) 
	{
		if(dataIn[i]->getStreamType() == STREAM_TYPE_RANGE)
		{
			c=(const RangeStreamData *)dataIn[i];

			dataOut.push_back(dataIn[i]);
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

		//Well, don't use per-species info anymore
		perSpecies=false;
	}
	else
	{


		//If we didn't have a previously incoming rsd, then make one up!
		// - we can't use a reference, as the rangestreams are technically transient,
		// so we have to copy.
		if(!rsdIncoming)
		{
			rsdIncoming = new RangeStreamData;
			*rsdIncoming=*c;

			if(ionFractions.size() != c->rangeFile->getNumIons())
			{
				//set up some defaults; seeded from normal
				ionFractions.resize(c->rangeFile->getNumIons(),fraction);
				ionLimits.resize(c->rangeFile->getNumIons(),maxAfterFilter);
			}
		}
		else
		{

			//OK, so we have a range incoming already (from last time)
			//-- the question is, is it the same one we had before ?
			//
			//Do a pointer comparison (its a hack, yes, but it should work)
			if(rsdIncoming->rangeFile != c->rangeFile)
			{
				//hmm, it is different. well, trash the old incoming rng
				delete rsdIncoming;

				rsdIncoming = new RangeStreamData;
				*rsdIncoming=*c;

				ionFractions.resize(c->rangeFile->getNumIons(),fraction);
				ionLimits.resize(c->rangeFile->getNumIons(),maxAfterFilter);
			}
			else if(ionFractions.size() !=c->rangeFile->getNumIons())
			{
				//well its the same range, but somehow the number of ions 
				//have changed. Could be range was reloaded.
				ionFractions.resize(rsdIncoming->rangeFile->getNumIons(),fraction);
				ionLimits.resize(rsdIncoming->rangeFile->getNumIons(),maxAfterFilter);
			}

			//Ensure what is enabled and is disabled is up-to-date	
			for(unsigned int ui=0;ui<rsdIncoming->enabledRanges.size();ui++)
				rsdIncoming->enabledRanges[ui] = c->enabledRanges[ui];
			for(unsigned int ui=0;ui<rsdIncoming->enabledIons.size();ui++)
				rsdIncoming->enabledIons[ui] = c->enabledIons[ui];
				
		}

	}


	ASSERT(ionLimits.size() == ionFractions.size());
}

Filter *IonDownsampleFilter::cloneUncached() const
{
	IonDownsampleFilter *p=new IonDownsampleFilter();
	p->rng = rng;
	p->maxAfterFilter=maxAfterFilter;
	p->fraction=fraction;
	p->perSpecies=perSpecies;
	p->rsdIncoming=rsdIncoming;

	p->ionFractions.resize(ionFractions.size());
	std::copy(ionFractions.begin(),ionFractions.end(),p->ionFractions.begin());
	p->ionLimits.resize(ionLimits.size());
	std::copy(ionLimits.begin(),ionLimits.end(),p->ionLimits.begin());


	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	p->fixedNumOut=fixedNumOut;
	return p;
}

size_t IonDownsampleFilter::numBytesForCache(size_t nObjects) const
{
	if(fixedNumOut)
	{
		if(nObjects > maxAfterFilter)
			return maxAfterFilter*IONDATA_SIZE;
		else
			return nObjects*IONDATA_SIZE;
	}
	else
	{
		return (size_t)((float)(nObjects*IONDATA_SIZE)*fraction);
	}
}

unsigned int IonDownsampleFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		for(size_t ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}
		for(size_t ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);
		return 0;
	}


	size_t totalSize = numElements(dataIn,STREAM_TYPE_IONS);
	if(!perSpecies)	
	{
		for(size_t ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS: 
				{
					if(!totalSize)
						continue;

					IonStreamData *d;
					d=new IonStreamData;
					d->parent=this;
					try
					{
						if(fixedNumOut)
						{
							float frac;
							frac = (float)(((const IonStreamData*)dataIn[ui])->data.size())/(float)totalSize;

							randomSelect(d->data,((const IonStreamData *)dataIn[ui])->data,
										rng,maxAfterFilter*frac,progress.filterProgress,callback,strongRandom);


						}
						else
						{

							unsigned int curProg=NUM_CALLBACK;
							size_t n=0;
							//Reserve 90% of storage needed.
							//highly likely with even modest numbers of ions
							//that this will be exceeeded
							d->data.reserve(fraction*0.9*totalSize);

							ASSERT(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS);

							for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
								       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
							{
								if(rng.genUniformDev() <  fraction)
									d->data.push_back(*it);
							
								//update progress every CALLBACK ions
								if(!curProg--)
								{
									curProg=NUM_CALLBACK;
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									if(!(*callback)(false))
									{
										delete d;
										return IONDOWNSAMPLE_ABORT_ERR;
									}
								}
							}
						}
					}
					catch(std::bad_alloc)
					{
						delete d;
						return IONDOWNSAMPLE_BAD_ALLOC;
					}

					//skip ion output sets wth no ions in them
					if(!d->data.size())
					{
						delete d;
						continue;
					}

					//Copy over other attributes
					d->r = ((IonStreamData *)dataIn[ui])->r;
					d->g = ((IonStreamData *)dataIn[ui])->g;
					d->b =((IonStreamData *)dataIn[ui])->b;
					d->a =((IonStreamData *)dataIn[ui])->a;
					d->ionSize =((IonStreamData *)dataIn[ui])->ionSize;
					d->representationType=((IonStreamData *)dataIn[ui])->representationType;
					d->valueType=((IonStreamData *)dataIn[ui])->valueType;

					//getOut is const, so shouldn't be modified
					if(cache)
					{
						d->cached=1;
						filterOutputs.push_back(d);
						cacheOK=true;
					}
					else
						d->cached=0;
			

					getOut.push_back(d);
					break;
				}
			
				default:
					getOut.push_back(dataIn[ui]);
					break;
			}

		}
	}
	else
	{
		ASSERT(rsdIncoming);
		const IonStreamData *input;

		//Construct two vectors. One with the ion IDs for each input
		//ion stream. the other with the total number of ions in the input
		//for each ion type 
		vector<size_t> numIons,ionIDVec;
		numIons.resize(rsdIncoming->rangeFile->getNumIons(),0);

		for(unsigned int uj=0;uj<dataIn.size() ;uj++)
		{
			if(dataIn[uj]->getStreamType() == STREAM_TYPE_IONS)
			{
				input=(const IonStreamData*)dataIn[uj];
				if(input->data.size())
				{
					unsigned int ionID;
					ionID=rsdIncoming->rangeFile->getIonID(
						input->data[0].getMassToCharge()); 

					if(ionID != (unsigned int)-1)
						numIons[ionID]+=input->data.size();
					
					ionIDVec.push_back(ionID);
				}
			}
		}

		size_t n=0;
		unsigned int idPos=0;
		for(size_t ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS: 
				{
					input=(const IonStreamData*)dataIn[ui];
			
					//Don't process ionstreams that are empty	
					if(input->data.empty())
						continue;

					//FIXME: Allow processing of unranged data
					//Don't process streams that are not ranged, as we cannot get their desired fractions
					//at this time
					if(ionIDVec[idPos]==(unsigned int)-1)
						continue;

					IonStreamData *d;
					d=new IonStreamData;
					d->parent=this;
					try
					{
						if(fixedNumOut)
						{
							//if we are building the fixed number for output,
							//then compute the relative fraction for this ion set
							float frac;
							frac = (float)(input->data.size())/(float)(numIons[ionIDVec[idPos]]);

							//The total number of ions is the specified value for this ionID, multiplied by
							//this stream's fraction of the total incoming data
							randomSelect(d->data,input->data, rng,frac*ionLimits[ionIDVec[idPos]],
									progress.filterProgress,callback,strongRandom);
						}
						else
						{
							//Use the direct fractions as entered in by user. 

							unsigned int curProg=NUM_CALLBACK;

							float thisFraction = ionFractions[ionIDVec[idPos]];
							
							//Reserve 90% of storage needed.
							//highly likely (poisson) with even modest numbers of ions
							//that this will be exceeeded, and thus we won't over-allocate
							d->data.reserve(thisFraction*0.9*numIons[ionIDVec[idPos]]);

							if(thisFraction)
							{
								for(vector<IonHit>::const_iterator it=input->data.begin();
									       it!=input->data.end(); ++it)
								{
									if(rng.genUniformDev() <  thisFraction)
										d->data.push_back(*it);
								
									//update progress every CALLBACK ions
									if(!curProg--)
									{
										n+=NUM_CALLBACK;
										progress.filterProgress= 
											(unsigned int)((float)(n)/((float)totalSize)*100.0f);
										if(!(*callback)(false))
										{
											delete d;
											return IONDOWNSAMPLE_ABORT_ERR;
										}
									}
								}
						
							}
						}
					}
					catch(std::bad_alloc)
					{
						delete d;
						return IONDOWNSAMPLE_BAD_ALLOC;
					}


					if(d->data.size())
					{
						//Copy over other attributes
						d->r = input->r;
						d->g = input->g;
						d->b =input->b;
						d->a =input->a;
						d->ionSize =input->ionSize;
						d->representationType=input->representationType;
						d->valueType=input->valueType;


						//getOut is const, so shouldn't be modified
						if(cache)
						{
							d->cached=1;
							filterOutputs.push_back(d);
							cacheOK=true;
						}
						else
							d->cached=0;
			

						getOut.push_back(d);
					}
					else
						delete d;
					//next ion
					idPos++;
					
					break;
				}
			
				default:
					getOut.push_back(dataIn[ui]);
					break;
			}

		}


	}	

	return 0;
}


void IonDownsampleFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	stream_cast(tmpStr,fixedNumOut);
	s.push_back(std::make_pair(TRANS("By Count"), tmpStr));
	keys.push_back(KEY_IONDOWNSAMPLE_FIXEDOUT);
	type.push_back(PROPERTY_TYPE_BOOL);

	if(rsdIncoming)
	{
		stream_cast(tmpStr,perSpecies);
		s.push_back(std::make_pair(TRANS("Per Species"), tmpStr));
		keys.push_back(KEY_IONDOWNSAMPLE_PERSPECIES);
		type.push_back(PROPERTY_TYPE_BOOL);
	}	


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	//Start a new section
	s.clear();
	type.clear();
	keys.clear();

	if(rsdIncoming && perSpecies)
	{
		unsigned int typeVal;
		if(fixedNumOut)
			typeVal=PROPERTY_TYPE_INTEGER;
		else
			typeVal=PROPERTY_TYPE_REAL;

		//create a  single line for each
		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{
			if(rsdIncoming->enabledIons[ui])
			{
				if(fixedNumOut)
					stream_cast(tmpStr,ionLimits[ui]);
				else
					stream_cast(tmpStr,ionFractions[ui]);

				s.push_back(make_pair(
					rsdIncoming->rangeFile->getName(ui), tmpStr));
				type.push_back(typeVal);
				keys.push_back(KEY_IONDOWNSAMPLE_DYNAMIC+ui);
			}
		}
	}
	else
	{
		if(fixedNumOut)
		{
			stream_cast(tmpStr,maxAfterFilter);
			keys.push_back(KEY_IONDOWNSAMPLE_COUNT);
			s.push_back(make_pair(TRANS("Output Count"), tmpStr));
			type.push_back(PROPERTY_TYPE_INTEGER);
		}
		else
		{
			stream_cast(tmpStr,fraction);
			s.push_back(make_pair(TRANS("Out Fraction"), tmpStr));
			keys.push_back(KEY_IONDOWNSAMPLE_FRACTION);
			type.push_back(PROPERTY_TYPE_REAL);


		}
	}
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool IonDownsampleFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{
	needUpdate=false;
	switch(key)
	{
		case KEY_IONDOWNSAMPLE_FIXEDOUT: 
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=fixedNumOut;
			fixedNumOut=(stripped=="1");

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=fixedNumOut)
			{
				needUpdate=true;
				clearCache();
			}

			break;
		}	
		case KEY_IONDOWNSAMPLE_FRACTION:
		{
			float newFraction;
			if(stream_cast(newFraction,value))
				return false;

			if(newFraction < 0.0f || newFraction > 1.0f)
				return false;

			//In the case of fixed number output, 
			//our cache is invalidated
			if(!fixedNumOut)
			{
				needUpdate=true;
				clearCache();
			}

			fraction=newFraction;
			

			break;
		}
		case KEY_IONDOWNSAMPLE_COUNT:
		{
			size_t count;

			if(stream_cast(count,value))
				return false;

			maxAfterFilter=count;
			//In the case of fixed number output, 
			//our cache is invalidated
			if(fixedNumOut)
			{
				needUpdate=true;
				clearCache();
			}
			
			break;
		}	
		case KEY_IONDOWNSAMPLE_PERSPECIES: 
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=perSpecies;

			perSpecies=(stripped=="1");

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=perSpecies)
			{
				needUpdate=true;
				clearCache();
			}

			break;
		}	
		default:
		{
			ASSERT(rsdIncoming);
			ASSERT(key >=KEY_IONDOWNSAMPLE_DYNAMIC);
			ASSERT(key < KEY_IONDOWNSAMPLE_DYNAMIC+ionLimits.size());
			ASSERT(ionLimits.size() == ionFractions.size());

			unsigned int offset;
			offset=key-KEY_IONDOWNSAMPLE_DYNAMIC;

			//TODO: Disable this test -
			// offset >=ionLimits.size()  did happen, but should not have. 
			// Can't reproduce bug - something to do with wrong filter being given selected properties in UI
			ASSERT( offset < ionLimits.size());
			if(offset >= ionLimits.size())
				return false;

			//Dynamically generated list of downsamples
			if(fixedNumOut)
			{
				//Fixed count
				size_t v;
				if(stream_cast(v,value))
					return false;
				ionLimits[offset]=v;
			}
			else
			{
				//Fixed fraction
				float v;
				if(stream_cast(v,value))
					return false;

				if(v < 0.0f || v> 1.0f)
					return false;

				ionFractions[offset]=v;
			}
			
			needUpdate=true;
			clearCache();
			break;
		}

	}	
	return true;
}


std::string  IonDownsampleFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case IONDOWNSAMPLE_ABORT_ERR:
			return std::string(TRANS("Downsample Aborted"));
		case IONDOWNSAMPLE_BAD_ALLOC:
			return std::string(TRANS("Insuffient memory for downsample"));
	}	

	return std::string("BUG! Should not see this (IonDownsample)");
}

bool IonDownsampleFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) <<  "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<fixednumout value=\""<<fixedNumOut<< "\"/>"  << endl;
			f << tabs(depth+1) << "<fraction value=\""<<fraction<< "\"/>"  << endl;
			f << tabs(depth+1) << "<maxafterfilter value=\"" << maxAfterFilter << "\"/>" << endl;
			f << tabs(depth+1) << "<perspecies value=\""<<perSpecies<< "\"/>"  << endl;
			f << tabs(depth+1) << "<fractions>" << endl;
			for(unsigned int ui=0;ui<ionFractions.size(); ui++) 
				f << tabs(depth+2) << "<scalar value=\"" << ionFractions[ui] << "\"/>" << endl; 
			f << tabs(depth+1) << "</fractions>" << endl;
			f << tabs(depth+1) << "<limits>" << endl;
			for(unsigned int ui=0;ui<ionLimits.size(); ui++)
				f << tabs(depth+2) << "<scalar value=\"" << ionLimits[ui] << "\"/>" << endl; 
			f << tabs(depth+1) << "</limits>" << endl;
			f << tabs(depth) << "</" <<trueName()<< ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonDownsampleFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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

	//Retrieve number out (yes/no) mode
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"fixednumout","value"))
		return false;
	
	if(tmpStr == "1") 
		fixedNumOut=true;
	else if(tmpStr== "0")
		fixedNumOut=false;
	else
		return false;
	//===
		
	//Retrieve Fraction
	//===
	if(!XMLGetNextElemAttrib(nodePtr,fraction,"fraction","value"))
		return false;
	//disallow negative or values gt 1.
	if(fraction < 0.0f || fraction > 1.0f)
		return false;
	//===
	
	
	//Retrieve "perspecies" attrib
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"perspecies","value"))
		return false;
	
	if(tmpStr == "1") 
		perSpecies=true;
	else if(tmpStr== "0")
		perSpecies=false;
	else
		return false;

	xmlNodePtr lastNode;
	lastNode=nodePtr;
	//Retrieve the ion per-species fractions
	if(XMLHelpFwdToElem(nodePtr,"fractions"))
		return false;

	nodePtr=nodePtr->xmlChildrenNode;

	//Populate the ion fraction vector
	float fracThis; 
	while(XMLGetNextElemAttrib(nodePtr,fracThis,"scalar","value"))
		ionFractions.push_back(fracThis);

	
	nodePtr=lastNode;

	//Retrieve the ion per-species fractions
	if(XMLHelpFwdToElem(nodePtr,"limits"))
		return false;

	nodePtr=nodePtr->xmlChildrenNode;
	size_t limitThis;	
	while(XMLGetNextElemAttrib(nodePtr,limitThis,"scalar","value"))
		ionLimits.push_back(limitThis);

	if(ionLimits.size()!=ionFractions.size())
		return false;

	return true;
}


unsigned int IonDownsampleFilter::getRefreshBlockMask() const
{
	return STREAM_TYPE_IONS ;
}

unsigned int IonDownsampleFilter::getRefreshEmitMask() const
{
	return  STREAM_TYPE_IONS;
}

unsigned int IonDownsampleFilter::getRefreshUseMask() const
{
	return  STREAM_TYPE_RANGE  | STREAM_TYPE_IONS;
}
//----------


//Unit testing for this class
///-----
#ifdef DEBUG

//Create a synthetic dataset of points
// returned pointer *must* be deleted. Span must have 3 elements, and for best results sould be co-prime with one another; eg all prime numbers
IonStreamData *synthDataPts(unsigned int span[],unsigned int numPts);

//Test for fixed number of output ions
bool fixedSampleTest();

//Test for variable number of output ions
bool variableSampleTest();

//Unit tests
bool IonDownsampleFilter::runUnitTests()
{
	if(!fixedSampleTest())
		return false;

	if(!variableSampleTest())
		return false;
	
	return true;
}

bool fixedSampleTest()
{
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d= new IonStreamData;

	const unsigned int NUM_PTS=10000;
	for(unsigned int ui=0;ui<NUM_PTS;ui++)
	{
		IonHit h;
		h.setPos(Point3D(ui,ui,ui));
		h.setMassToCharge(ui);
		d->data.push_back(h);
	}


	streamIn.push_back(d);
	//Set up the filter itself
	IonDownsampleFilter *f=new IonDownsampleFilter;
	f->setCaching(false);

	bool needUp;
	string s;
	unsigned int numOutput=NUM_PTS/10;
	
	f->setProperty(0,KEY_IONDOWNSAMPLE_FIXEDOUT,"1",needUp);
	stream_cast(s,numOutput);
	f->setProperty(0,KEY_IONDOWNSAMPLE_COUNT,s,needUp);

	//Do the refresh
	ProgressData p;
	f->refresh(streamIn,streamOut,p,dummyCallback);

	delete f;
	delete d;

	//Pass some tests
	TEST(streamOut.size() == 1, "Stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS, "stream type");
	TEST(streamOut[0]->getNumBasicObjects() == numOutput, "output ions (basicobject)"); 
	TEST( ((IonStreamData*)streamOut[0])->data.size() == numOutput, "output ions (direct)")

	delete streamOut[0];
	
	return true;
}

bool variableSampleTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	unsigned int span[]={ 
			5, 7, 9
			};	
	const unsigned int NUM_PTS=10000;
	IonStreamData *d=synthDataPts(span,NUM_PTS);

	streamIn.push_back(d);
	IonDownsampleFilter *f=new IonDownsampleFilter;
	f->setCaching(false);	
	
	bool needUp;
	f->setProperty(0,KEY_IONDOWNSAMPLE_FIXEDOUT,"0",needUp);
	f->setProperty(0,KEY_IONDOWNSAMPLE_FRACTION,"0.1",needUp);

	//Do the refresh
	ProgressData p;
	TEST(!(f->refresh(streamIn,streamOut,p,dummyCallback)),"refresh error code");

	delete f;
	delete d;


	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");

	//It is HIGHLY improbable that it will be <1/10th of the requested number
	TEST(streamOut[0]->getNumBasicObjects() > 0.01*NUM_PTS 
		&& streamOut[0]->getNumBasicObjects() <= NUM_PTS,"ion fraction");

	delete streamOut[0];


	return true;
}

IonStreamData *synthDataPts(unsigned int span[], unsigned int numPts)
{
	IonStreamData *d = new IonStreamData;
	
	for(unsigned int ui=0;ui<numPts;ui++)
	{
		IonHit h;
		h.setPos(Point3D(ui%span[0],
			ui%span[1],ui%span[2]));
		h.setMassToCharge(ui);
		d->data.push_back(h);
	}

	return d;
}

#endif
///-----

